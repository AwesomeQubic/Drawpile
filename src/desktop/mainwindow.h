// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

extern "C" {
#include <dpengine/load.h>
}

#include <QMainWindow>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QMap>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include "desktop/dialogs/flipbook.h"
#include "desktop/scene/toggleitem.h"
#include "libclient/tools/tool.h"
#include "libclient/canvas/acl.h"
#include "libclient/drawdance/canvasstate.h"
#include "libclient/export/animationsaverrunnable.h"

class QActionGroup;
class QLabel;
class QShortcutEvent;
class QSplitter;
class QToolButton;

class Document;
class MainActions;
class ActionBuilder;

namespace widgets {
	class CanvasView;
	class NetStatus;
	class ChatBox;
	class ViewStatus;
	class ViewStatusBar;
}
namespace docks {
	class ToolSettings;
	class BrushPalette;
	class InputSettings;
	class LayerList;
	class PaletteBox;
	class ColorPaletteDock;
	class ColorSpinnerDock;
	class ColorSliderDock;
	class Navigator;
	class OnionSkinsDock;
	class Timeline;
}
namespace dialogs {
	class DumpPlaybackDialog;
	class PlaybackDialog;
	class HostDialog;
	class SessionSettingsDialog;
	class ServerLogDialog;
	class SettingsDialog;
	class StartDialog;
}
namespace drawingboard {
	class CanvasScene;
}
namespace canvas {
	class CanvasModel;
	class Selection;
}
namespace drawdance {
	class CanvasState;
}
namespace desktop {
namespace settings {
class Settings;
}
}

class ShortcutDetector;

//! The application main window
class MainWindow final : public QMainWindow {
	Q_OBJECT
public:
	MainWindow(bool restoreWindowPosition=true);
	~MainWindow() override;

	void autoJoin(const QUrl &url);

	void hostSession(
		const QString &title, const QString &password, const QString &alias,
		bool nsfm, const QString &announcementUrl, const QString &remoteAddress);

	//! Connect to a host and join a session if full URL is provided.
	void joinSession(const QUrl& url, const QString &autoRecordFilename=QString());

	//! Check if the current board can be replaced
	bool canReplace() const;

	//! Save settings and exit
	void exit();

	dialogs::StartDialog *showStartDialog();

	void showPopupMessage(const QString &message);

	bool notificationsMuted() const { return m_notificationsMuted; }
	bool isInitialCatchup() const { return m_initialCatchup; }

signals:
	void hostSessionEnabled(bool enabled);
	void windowReplacementFailed(MainWindow *win);

public slots:
	// Triggerable actions
	void start();
	void showNew();
	void open();
	void open(const QUrl &url);
	bool save();
	void saveas();
	void saveSelection();
	void exportImage();
	void importOldAnimation();
	void showFlipbook();

	void showBrushSettingsDialog();
	dialogs::SettingsDialog *showSettings();
	void reportAbuse();
	void tryToGainOp();
	void resetSession();
	void terminateSession();

	void host();
	void invite();
	void join();
	void browse();
	void leave();
	void checkForUpdates();

	void toggleFullscreen();
	void setShowAnnotations(bool show);
	void setShowLaserTrails(bool show);

	void selectTool(QAction *tool);

	static void about();
	static void homepage();

	//! Create a blank new document
	void newDocument(const QSize &size, const QColor &background);

private slots:
	void toggleRecording();
	void toggleProfile();
	void toggleTabletEventLog();

	void exportTemplate();
	void exportGifAnimation();
	void exportGifAnimationWith(
		const drawdance::CanvasState &canvasState, const QRect &crop, int start,
		int end, int framerate);
	// On Android, we can only save stuff to individual files that the user
	// specifies, we're not allowed to spew multiple files into a directory.
#ifndef Q_OS_ANDROID
	void exportAnimationFrames();
	void exportAnimationFramesWith(
		const drawdance::CanvasState &canvasState, const QRect &crop, int start,
		int end);
#endif

	void showResetNoticeDialog(const drawdance::CanvasState &canvasState);
	void updateCatchupProgress(int percent);
	void savePreResetImageAs();
	void discardPreResetImage();
	void showCompatibilityModeWarning();

	void onOperatorModeChange(bool op);
	void onFeatureAccessChange(DP_Feature feature, bool canUse);
	void onUndoDepthLimitSet(int undoDepthLimit);

	void onServerConnected();
	void onServerLogin(bool join, const QString &joinPassword);
	void onServerDisconnected(const QString &message, const QString &errorcode, bool localDisconnect);
	void onCompatibilityModeChanged(bool compatibilityMode);
	void onNsfmChanged(bool nsfm);

	void updateLockWidget();
	void setRecorderStatus(bool on);

	void loadShortcuts(const QVariantMap &shortcuts);

	void toggleLayerViewMode();
	void updateLayerViewMode();

	void copyText();
	void paste();
	void pasteCentered();
	void pasteFile();
	void pasteFile(const QUrl &url);
	void pasteImage(const QImage &image, const QPoint *point=nullptr, bool force=false);
	void dropUrl(const QUrl &url);

	void clearOrDelete();

	void resizeCanvas();
	void updateBackgroundActions();
	void changeCanvasBackground();
	void changeLocalCanvasBackground();
	void clearLocalCanvasBackground();

	void showLayoutsDialog();
	void showUserInfoDialog(int userId);

	void changeUndoDepthLimit();

	void updateDevToolsActions();
	void setArtificialLag();
	void setArtificialDisconnect();
	void toggleDebugDump();
	void openDebugDump();

	void toolChanged(tools::Tool::Type tool);
	void updateFreehandToolButton(int brushMode);
	void handleFreehandToolButtonClicked();

	void activeAnnotationChanged(int annotationId);
	void selectionChanged(canvas::Selection *selection);
	void selectionRemoved();

	void setFreezeDocks(bool freeze);
	void setDocksHidden(bool hidden);
	void setDockTitleBarsHidden(bool hidden);
	void handleToggleAction(drawingboard::ToggleItem::Action action);
	void setNotificationsMuted(bool muted);

	void updateTitle();

	void onCanvasChanged(canvas::CanvasModel *canvas);
	void onCanvasSaveStarted();
	void onCanvasSaved(const QString &errorMessage);
	void onTemplateExported(const QString &errorMessage);

	bool eventFilter(QObject *object, QEvent *event) override;

protected:
	void closeEvent(QCloseEvent *event) override;
	bool event(QEvent *event) override;

private:
	MainWindow *replaceableWindow();

	void connectStartDialog(dialogs::StartDialog *dlg);
	void setStartDialogActions(dialogs::StartDialog *dlg);
	void closeStartDialog(dialogs::StartDialog *dlg);
	QWidget *getStartDialogOrThis();

	void exportAnimation(
		const drawdance::CanvasState &canvasState, const QString &path,
		AnimationSaverRunnable::SaveFn saveFn);

	ActionBuilder makeAction(const char *name, const QString &text = QString{});
	QAction *getAction(const QString &name);

	//! Add a new entry to recent files list
	void addRecentFile(const QString& file);

	//! Enable or disable drawing tools
	void setDrawingToolsEnabled(bool enable);

	//! Display an error message
	void showErrorMessage(const QString &message);
	void showErrorMessageWithDetails(const QString &message, const QString &details);
	void showLoadResultMessage(DP_LoadResult result);
	void handleAmbiguousShortcut(QShortcutEvent *shortcutEvent);

	void readSettings(bool windowpos=true);
	void restoreSettings(const desktop::settings::Settings &settings);
	void initSmallScreenState();
	void initDefaultDocks();
	void setDefaultDockSizes();
	void saveSplitterState();
	void saveWindowState();

	void requestUserInfo(int userId);
	void sendUserInfo(int userId);
	void requestCurrentBrush(int userId);
	void sendCurrentBrush(int userId, const QString &correlator);
	void receiveCurrentBrush(int userId, const QJsonObject &info);

	void createDocks();
	void setupActions();

	QDeadlineTimer m_lastDisconnectNotificationTimer;

	QTimer m_saveWindowDebounce;
	QTimer m_saveSplitterDebounce;
	QMap<QString, bool> m_actionsConfig;

	QSplitter *m_splitter;

	docks::ToolSettings *m_dockToolSettings;
	docks::BrushPalette *m_dockBrushPalette;
	docks::InputSettings *m_dockInput;
	docks::LayerList *m_dockLayers;
	docks::ColorPaletteDock *m_dockColorPalette;
	docks::ColorSpinnerDock *m_dockColorSpinner;
	docks::ColorSliderDock *m_dockColorSliders;
	docks::Navigator *m_dockNavigator;
	docks::OnionSkinsDock *m_dockOnionSkins;
	docks::Timeline *m_dockTimeline;
	QToolBar *m_toolBarDraw;
	QAction *m_freehandAction;
	QToolButton *m_freehandButton;
	QByteArray m_hiddenDockState;
	widgets::ChatBox *m_chatbox;

	widgets::CanvasView *m_view;

	widgets::ViewStatusBar *m_viewStatusBar;
	QLabel *m_lockstatus;
	widgets::NetStatus *m_netstatus;
	widgets::ViewStatus *m_viewstatus;
	QToolButton *m_statusChatButton;

	QPointer<dialogs::PlaybackDialog> m_playbackDialog;
	QPointer<dialogs::DumpPlaybackDialog> m_dumpPlaybackDialog;
	dialogs::SessionSettingsDialog *m_sessionSettings;
	dialogs::ServerLogDialog *m_serverLogDialog;
	dialogs::Flipbook::State m_flipbookState;

	drawingboard::CanvasScene *m_canvasscene;

	QMenu *m_recentMenu;
	QAction *m_lastLayerViewMode;

	QActionGroup *m_currentdoctools; // general tools that require no special permissions
	QActionGroup *m_admintools;      // session operator actions
	QActionGroup *m_canvasbgtools;   // tools related to canvas background feature
	QActionGroup *m_resizetools;     // tools related to canvas resizing feature
	QActionGroup *m_putimagetools;   // Cut&Paste related tools
	QActionGroup *m_undotools;       // undo&redo related tools
	QActionGroup *m_drawingtools;    // drawing tool selection
	QActionGroup *m_brushSlots;      // tool slot shortcuts
	QActionGroup *m_dockToggles;     // dock visibility toggles

	int m_lastToolBeforePaste; // Last selected tool before Paste was used

	QMetaObject::Connection m_textCopyConnection;

#ifndef Q_OS_ANDROID
	// Remember window state to return from fullscreen mode
	QRect m_fullscreenOldGeometry;
	bool m_fullscreenOldMaximized;
#endif

	QElapsedTimer m_toolChangeTime; // how long the user has held down the tool change button
	ShortcutDetector *m_tempToolSwitchShortcut;
	bool m_titleBarsHidden;
	bool m_wasSessionLocked;
	bool m_notificationsMuted;
	bool m_initialCatchup;

	Document *m_doc;
	MainActions *m_ma;
	enum { RUNNING, DISCONNECTING, SAVING } m_exitAction;

	drawdance::CanvasState m_preResetCanvasState;

	int m_brushRequestUserId = -1;
	QString m_brushRequestCorrelator;
	QElapsedTimer m_brushRequestTime;
};

#endif

