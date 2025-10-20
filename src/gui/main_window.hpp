#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../models.hpp"

// GUI callback types
using FileSelectedCallback = std::function<void(const std::string&)>;
using SyncButtonCallback = std::function<void()>;
using SettingsChangedCallback = std::function<void(const std::string&, const std::string&)>;

// Statistics display data
struct GUIStatistics {
    size_t totalFiles = 0;
    size_t syncedFiles = 0;
    size_t activePeers = 0;
    size_t totalPeers = 0;
    double uploadRate = 0.0;    // MB/s
    double downloadRate = 0.0;  // MB/s
    size_t bytesTransferred = 0;
    std::string lastSync;
    double mlAccuracy = 0.0;
    size_t anomaliesDetected = 0;
};

// Main application window
class MainWindow {
public:
    MainWindow(int argc, char* argv[]);
    ~MainWindow();
    
    // Window lifecycle
    void show();
    void run();
    void close();
    
    // UI updates (thread-safe)
    void updateFileList(const std::vector<FileInfo>& files);
    void updatePeerList(const std::vector<PeerInfo>& peers);
    void updateStatistics(const GUIStatistics& stats);
    void addLogMessage(const std::string& message, const std::string& level = "INFO");
    
    // Callbacks
    void setFileSelectedCallback(FileSelectedCallback callback);
    void setSyncButtonCallback(SyncButtonCallback callback);
    void setSettingsChangedCallback(SettingsChangedCallback callback);
    
    // Status updates
    void setStatus(const std::string& status, bool isError = false);
    void showNotification(const std::string& title, const std::string& message);
    
private:
    // GTK widgets
    GtkWidget* window;
    GtkWidget* headerBar;
    GtkWidget* mainBox;
    GtkWidget* notebook;
    
    // Tab 1: Files
    GtkWidget* filesScrolled;
    GtkWidget* filesTreeView;
    GtkListStore* filesListStore;
    
    // Tab 2: Peers
    GtkWidget* peersScrolled;
    GtkWidget* peersTreeView;
    GtkListStore* peersListStore;
    
    // Tab 3: Statistics
    GtkWidget* statsBox;
    GtkWidget* statsLabels[10];
    GtkWidget* progressBar;
    
    // Tab 4: Logs
    GtkWidget* logsScrolled;
    GtkWidget* logsTextView;
    GtkTextBuffer* logsTextBuffer;
    
    // Toolbar
    GtkWidget* toolbar;
    GtkWidget* syncButton;
    GtkWidget* pauseButton;
    GtkWidget* settingsButton;
    GtkWidget* aboutButton;
    
    // Status bar
    GtkWidget* statusBar;
    guint statusContextId;
    
    // Callbacks
    FileSelectedCallback fileSelectedCallback;
    SyncButtonCallback syncButtonCallback;
    SettingsChangedCallback settingsChangedCallback;
    
    // Private methods
    void createUI();
    void createHeaderBar();
    void createNotebook();
    void createFilesTab();
    void createPeersTab();
    void createStatsTab();
    void createLogsTab();
    void createToolbar();
    void createStatusBar();
    void setupStyles();
    
    // Signal handlers
    static void onSyncButtonClicked(GtkWidget* widget, gpointer data);
    static void onPauseButtonClicked(GtkWidget* widget, gpointer data);
    static void onSettingsButtonClicked(GtkWidget* widget, gpointer data);
    static void onAboutButtonClicked(GtkWidget* widget, gpointer data);
    static void onFileSelected(GtkTreeSelection* selection, gpointer data);
    static gboolean onWindowDelete(GtkWidget* widget, GdkEvent* event, gpointer data);
    
    // Helper methods
    void appendLog(const std::string& message, const std::string& level);
    std::string formatBytes(size_t bytes);
    std::string formatRate(double mbps);
};

// Settings dialog
class SettingsDialog {
public:
    SettingsDialog(GtkWindow* parent);
    ~SettingsDialog();
    
    void show();
    void setConfig(const std::string& key, const std::string& value);
    std::string getConfig(const std::string& key);
    
private:
    GtkWidget* dialog;
    GtkWidget* notebook;
    
    // General settings
    GtkWidget* syncPathEntry;
    GtkWidget* sessionCodeEntry;
    GtkWidget* autoStartCheckbox;
    
    // Sync settings
    GtkWidget* uploadLimitSpin;
    GtkWidget* downloadLimitSpin;
    GtkWidget* enableVersioningCheckbox;
    GtkWidget* maxVersionsSpin;
    
    // ML settings
    GtkWidget* enableMLCheckbox;
    GtkWidget* anomalyThresholdScale;
    GtkWidget* enableFederatedCheckbox;
    
    void createGeneralTab();
    void createSyncTab();
    void createMLTab();
    
    static void onDialogResponse(GtkDialog* dialog, gint response_id, gpointer data);
};

// About dialog
void showAboutDialog(GtkWindow* parent);
