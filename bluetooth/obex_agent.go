/*
 * Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
 *
 * Author:     zhihsian <i@zhihsian.me>
 *
 * Maintainer: zhihsian <i@zhihsian.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package bluetooth

import (
	"errors"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"sync"
	"time"

	obex "github.com/linuxdeepin/go-dbus-factory/org.bluez.obex"
	notifications "github.com/linuxdeepin/go-dbus-factory/org.freedesktop.notifications"
	dbus "pkg.deepin.io/lib/dbus1"
	"pkg.deepin.io/lib/dbusutil"
	"pkg.deepin.io/lib/gettext"
	"pkg.deepin.io/lib/xdg/userdir"
)

const (
	obexAgentDBusPath      = dbusPath + "/ObexAgent"
	obexAgentDBusInterface = "org.bluez.obex.Agent1"

	receiveFileNotifyTimeout = 60 * 1000
)

var receiveBaseDir = userdir.Get(userdir.Download)

type obexAgent struct {
	b *Bluetooth

	service *dbusutil.Service
	sigLoop *dbusutil.SignalLoop

	obexManager *obex.Manager

	notifyCh           chan bool
	acceptedSessions   map[dbus.ObjectPath]int
	acceptedSessionsMu sync.Mutex

	methods *struct {
		AuthorizePush func() `in:"transferPath" out:"filename"`
		Cancel        func()
	}
}

func (*obexAgent) GetInterfaceName() string {
	return obexAgentDBusInterface
}

func newObexAgent(service *dbusutil.Service, bluetooth *Bluetooth) *obexAgent {
	return &obexAgent{
		b:                bluetooth,
		service:          service,
		acceptedSessions: make(map[dbus.ObjectPath]int),
	}
}

func (a *obexAgent) init() {
	sessionBus := a.service.Conn()
	a.obexManager = obex.NewManager(sessionBus)
	a.registerAgent()

	a.sigLoop = dbusutil.NewSignalLoop(a.service.Conn(), 0)
	a.sigLoop.Start()

}

// registerAgent 注册 OBEX 的代理
func (a *obexAgent) registerAgent() {
	err := a.obexManager.RegisterAgent(0, obexAgentDBusPath)
	if err != nil {
		logger.Error("failed to register obex agent:", err)
	}
}

// unregisterAgent 注销 OBEX 的代理
func (a *obexAgent) unregisterAgent() {
	err := a.obexManager.UnregisterAgent(0, obexAgentDBusPath)
	if err != nil {
		logger.Error("failed to unregister obex agent:", err)
	}
}

// AuthorizePush 用于请求用户接收文件
func (a *obexAgent) AuthorizePush(transferPath dbus.ObjectPath) (string, *dbus.Error) {
	transfer, err := obex.NewTransfer(a.service.Conn(), transferPath)
	if err != nil {
		logger.Error("failed to new transfer:", err)
		return "", dbusutil.ToError(err)
	}

	filename, err := transfer.Name().Get(0)
	if err != nil {
		logger.Warning("failed to get filename:", err)
		return "", dbusutil.ToError(err)
	}

	sessionPath, err := transfer.Session().Get(0)
	if err != nil {
		logger.Warning("failed to get transfer session path:", err)
		return "", dbusutil.ToError(err)
	}
	logger.Debug("session path:", sessionPath)

	session, err := obex.NewSession(a.service.Conn(), sessionPath)
	if err != nil {
		logger.Warning("failed to get transfer session:", err)
		return "", dbusutil.ToError(err)
	}

	deviceAddress, err := session.Destination().Get(0)
	if err != nil {
		logger.Warning("failed to get device address:", err)
		return "", dbusutil.ToError(err)
	}

	dev := a.b.getConnectedDeviceByAddress(deviceAddress)
	if dev == nil {
		err = errors.New("failed to get device info")
		logger.Error(err)
		return "", dbusutil.ToError(err)
	}

	deviceName := dev.Alias
	if len(deviceName) == 0 {
		deviceName = dev.Name
	}

	accepted, err := a.isSessionAccepted(sessionPath, deviceName)
	if err != nil {
		return "", dbusutil.ToError(err)
	}
	if !accepted {
		return "", dbusutil.ToError(errors.New("declined"))
	}

	a.receiveProgress(deviceName, sessionPath, transfer)

	return filename, nil
}

func (a *obexAgent) isSessionAccepted(sessionPath dbus.ObjectPath, deviceName string) (bool, error) {
	a.acceptedSessionsMu.Lock()
	defer a.acceptedSessionsMu.Unlock()

	_, accepted := a.acceptedSessions[sessionPath]
	if !accepted {
		var err error
		accepted, err = a.requestReceive(deviceName)
		if err != nil {
			return false, err
		}

		if !accepted {
			return false, nil
		}

		a.acceptedSessions[sessionPath] = 0
	}

	a.acceptedSessions[sessionPath]++
	return true, nil
}

func (a *obexAgent) receiveProgress(device string, sessionPath dbus.ObjectPath, transfer *obex.Transfer) {
	transfer.InitSignalExt(a.sigLoop, true)

	fileSize, err := transfer.Size().Get(0)
	if err != nil {
		logger.Error("failed to get file size:", err)
	}

	notify := notifications.NewNotifications(a.service.Conn())
	notify.InitSignalExt(a.sigLoop, true)

	var notifyMu sync.Mutex
	var notifyID uint32
	var oriFilepath string
	var basename string

	err = transfer.Status().ConnectChanged(func(hasValue bool, value string) {
		if !hasValue {
			return
		}

		// 传送开始，获取到保存的绝对路径
		if value == transferStatusActive {
			oriFilepath, err = transfer.Filename().Get(0)
			if err != nil {
				logger.Error("failed to get received filename:", err)
				return
			}

			basename = filepath.Base(oriFilepath)
			notifyMu.Lock()
			notifyID = a.notifyProgress(notify, notifyID, basename, device, 0)
			notifyMu.Unlock()
		}

		if value != transferStatusComplete && value != transferStatusError {
			return
		}

		// 手机会在一个传送完成之后再开始下一个传送，所以 transfer path 会一样
		transfer.RemoveAllHandlers()

		notify.RemoveAllHandlers()

		if value == transferStatusComplete {
			// 传送完成，移动到下载目录
			basename := filepath.Base(oriFilepath)
			dest := filepath.Join(receiveBaseDir, basename)
			err = os.Rename(oriFilepath, dest)
			if err != nil {
				logger.Error("failed to move file:", err)
			}

			notifyMu.Lock()
			notifyID = a.notifyProgress(notify, notifyID, basename, device, 100)
			notifyMu.Unlock()
		} else {
			notifyMu.Lock()
			notifyID = a.notifyFailed(notify, notifyID)
			notifyMu.Unlock()
		}

		// 避免下个传输还没开始就被清空，导致需要重新询问，故加上一秒的延迟
		time.AfterFunc(time.Second, func() {
			a.acceptedSessionsMu.Lock()
			a.acceptedSessions[sessionPath]--
			if a.acceptedSessions[sessionPath] == 0 {
				delete(a.acceptedSessions, sessionPath)
			}
			a.acceptedSessionsMu.Unlock()
		})
	})
	if err != nil {
		logger.Warning("connect status changed failed:", err)
	}

	var progress uint64 = math.MaxUint64
	err = transfer.Transferred().ConnectChanged(func(hasValue bool, value uint64) {
		if !hasValue {
			return
		}

		newProgress := value * 100 / fileSize
		if progress == newProgress {
			return
		}

		progress = newProgress
		logger.Infof("transferPath: %s, progress: %d", transfer.Path_(), progress)

		notifyMu.Lock()
		notifyID = a.notifyProgress(notify, notifyID, basename, device, progress)
		notifyMu.Unlock()
	})
	if err != nil {
		logger.Warning("connect transferred changed failed:", err)
	}

	_, err = notify.ConnectActionInvoked(func(id uint32, actionKey string) {
		notifyMu.Lock()
		if notifyID != id {
			notifyMu.Unlock()
			return
		}
		notifyMu.Unlock()

		if actionKey != "cancel" {
			return
		}

		err := transfer.Cancel(0)
		if err != nil {
			logger.Warning("failed to cancel transfer:", err)
		}
	})
	if err != nil {
		logger.Warning("connect action invoked failed:", err)
	}
}

// notifyProgress 发送文件传输进度通知
func (a *obexAgent) notifyProgress(notify *notifications.Notifications, replaceID uint32, filename string, device string, progress uint64) uint32 {
	var actions []string
	if progress != 100 {
		actions = []string{"cancel", gettext.Tr("Cancel")}
	}

	notifyID, err := notify.Notify(0,
		"dde-control-center",
		replaceID,
		notifyIconBluetoothConnected,
		fmt.Sprintf(gettext.Tr("Receiving %s files from \"%s\""), filename, device),
		fmt.Sprintf("%d%%", progress),
		actions,
		nil,
		receiveFileNotifyTimeout)
	if err != nil {
		logger.Warning("failed to send notify:", err)
	}

	return notifyID
}

// notifyFailed 发送文件传输失败通知
func (a *obexAgent) notifyFailed(notify *notifications.Notifications, replaceID uint32) uint32 {
	notifyID, err := notify.Notify(0,
		"dde-control-center",
		replaceID,
		notifyIconBluetoothConnectFailed,
		gettext.Tr("File Transfer Failed"),
		gettext.Tr("The Bluetooth device is disconnected, please have a check"),
		nil,
		nil,
		receiveFileNotifyTimeout)
	if err != nil {
		logger.Warning("failed to send notify:", err)
	}

	return notifyID
}

// Cancel 用于在客户端取消发送文件时取消文件传输请求
func (a *obexAgent) Cancel() *dbus.Error {
	if a.notifyCh == nil {
		return dbusutil.ToError(errors.New("no such process"))
	}

	a.notifyCh <- false

	return nil
}

// requestReceive 询问用户是否接收文件
func (a *obexAgent) requestReceive(deviceName string) (bool, error) {
	notify := notifications.NewNotifications(a.service.Conn())
	notify.InitSignalExt(a.sigLoop, true)

	actions := []string{"decline", gettext.Tr("Decline"), "receive", gettext.Tr("Receive")}
	notifyID, err := notify.Notify(0,
		"dde-control-center",
		0,
		notifyIconBluetoothConnected,
		gettext.Tr("Bluetooth File Transfer"),
		fmt.Sprintf(gettext.Tr(`"%s" wants to send files to you. Receive?`), deviceName),
		actions,
		nil,
		receiveFileNotifyTimeout)
	if err != nil {
		logger.Warning("failed to send notify:", err)
		return false, err
	}

	a.notifyCh = make(chan bool)

	_, err = notify.ConnectActionInvoked(func(id uint32, actionKey string) {
		if notifyID != id {
			return
		}

		if actionKey == "receive" {
			a.notifyCh <- true
			return
		}

		a.notifyCh <- false
	})
	if err != nil {
		logger.Warning("ConnectActionInvoked failed:", err)
		return false, dbusutil.ToError(err)
	}

	_, err = notify.ConnectNotificationClosed(func(id uint32, reason uint32) {
		if notifyID != id {
			return
		}

		a.notifyCh <- false
	})
	if err != nil {
		logger.Warning("ConnectNotificationClosed failed:", err)
		return false, dbusutil.ToError(err)
	}

	result := <-a.notifyCh
	a.notifyCh = nil

	notify.RemoveAllHandlers()

	return result, nil
}
