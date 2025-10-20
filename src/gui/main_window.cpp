#include "main_window.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

// File list columns
enum {
    COL_FILE_PATH = 0,
    COL_FILE_SIZE,
    COL_FILE_MODIFIED,
    COL_FILE_STATUS,
    COL_FILE_COUNT
};

// Peer list columns
enum {
    COL_PEER_ID = 0,
    COL_PEER_ADDRESS,
    COL_PEER_PORT,
    COL_PEER_LATENCY,
    COL_PEER_STATUS,
    COL_PEER_COUNT
};

MainWindow::MainWindow(int argc, char* argv[]) {
    gtk_init(&argc, &argv);
    
    // Initialize widgets
    window = nullptr;
    headerBar = nullptr;
    filesListStore = nullptr;
    peersListStore = nullptr;
    logsTextBuffer = nullptr;
    
    createUI();
}

MainWindow::~MainWindow() {
    // GTK handles cleanup
}

void MainWindow::createUI() {
    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "SentinelFS-Neo");
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    
    // Connect destroy signal
    g_signal_connect(window, "delete-event", G_CALLBACK(onWindowDelete), this);
    
    // Create main vertical box
    mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), mainBox);
    
    // Create components
    createHeaderBar();
    createToolbar();
    createNotebook();
    createStatusBar();
    
    // Apply styles
    setupStyles();
}

void MainWindow::createHeaderBar() {
    headerBar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(headerBar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(headerBar), "SentinelFS-Neo");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(headerBar), "Distributed P2P File Sync");
    gtk_window_set_titlebar(GTK_WINDOW(window), headerBar);
}

void MainWindow::createToolbar() {
    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_box_pack_start(GTK_BOX(mainBox), toolbar, FALSE, FALSE, 0);
    
    // Sync button
    GtkToolItem* syncItem = gtk_tool_button_new(nullptr, "Sync Now");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(syncItem), "view-refresh");
    g_signal_connect(syncItem, "clicked", G_CALLBACK(onSyncButtonClicked), this);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), syncItem, -1);
    syncButton = GTK_WIDGET(syncItem);
    
    // Pause button
    GtkToolItem* pauseItem = gtk_tool_button_new(nullptr, "Pause");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(pauseItem), "media-playback-pause");
    g_signal_connect(pauseItem, "clicked", G_CALLBACK(onPauseButtonClicked), this);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), pauseItem, -1);
    pauseButton = GTK_WIDGET(pauseItem);
    
    // Separator
    GtkToolItem* sep1 = gtk_separator_tool_item_new();
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), sep1, -1);
    
    // Settings button
    GtkToolItem* settingsItem = gtk_tool_button_new(nullptr, "Settings");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(settingsItem), "preferences-system");
    g_signal_connect(settingsItem, "clicked", G_CALLBACK(onSettingsButtonClicked), this);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), settingsItem, -1);
    settingsButton = GTK_WIDGET(settingsItem);
    
    // About button
    GtkToolItem* aboutItem = gtk_tool_button_new(nullptr, "About");
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(aboutItem), "help-about");
    g_signal_connect(aboutItem, "clicked", G_CALLBACK(onAboutButtonClicked), this);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), aboutItem, -1);
    aboutButton = GTK_WIDGET(aboutItem);
}

void MainWindow::createNotebook() {
    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(mainBox), notebook, TRUE, TRUE, 0);
    
    createFilesTab();
    createPeersTab();
    createStatsTab();
    createLogsTab();
}

void MainWindow::createFilesTab() {
    // Create scrolled window
    filesScrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(filesScrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // Create list store
    filesListStore = gtk_list_store_new(COL_FILE_COUNT,
                                       G_TYPE_STRING,  // Path
                                       G_TYPE_STRING,  // Size
                                       G_TYPE_STRING,  // Modified
                                       G_TYPE_STRING); // Status
    
    // Create tree view
    filesTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(filesListStore));
    gtk_container_add(GTK_CONTAINER(filesScrolled), filesTreeView);
    
    // Add columns
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(filesTreeView),
                                               -1, "File Path", renderer,
                                               "text", COL_FILE_PATH, nullptr);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(filesTreeView),
                                               -1, "Size", renderer,
                                               "text", COL_FILE_SIZE, nullptr);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(filesTreeView),
                                               -1, "Modified", renderer,
                                               "text", COL_FILE_MODIFIED, nullptr);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(filesTreeView),
                                               -1, "Status", renderer,
                                               "text", COL_FILE_STATUS, nullptr);
    
    // Add to notebook
    GtkWidget* label = gtk_label_new("📁 Files");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), filesScrolled, label);
}

void MainWindow::createPeersTab() {
    // Create scrolled window
    peersScrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(peersScrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // Create list store
    peersListStore = gtk_list_store_new(COL_PEER_COUNT,
                                       G_TYPE_STRING,  // ID
                                       G_TYPE_STRING,  // Address
                                       G_TYPE_INT,     // Port
                                       G_TYPE_STRING,  // Latency
                                       G_TYPE_STRING); // Status
    
    // Create tree view
    peersTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(peersListStore));
    gtk_container_add(GTK_CONTAINER(peersScrolled), peersTreeView);
    
    // Add columns
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(peersTreeView),
                                               -1, "Peer ID", renderer,
                                               "text", COL_PEER_ID, nullptr);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(peersTreeView),
                                               -1, "Address", renderer,
                                               "text", COL_PEER_ADDRESS, nullptr);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(peersTreeView),
                                               -1, "Port", renderer,
                                               "text", COL_PEER_PORT, nullptr);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(peersTreeView),
                                               -1, "Latency", renderer,
                                               "text", COL_PEER_LATENCY, nullptr);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(peersTreeView),
                                               -1, "Status", renderer,
                                               "text", COL_PEER_STATUS, nullptr);
    
    // Add to notebook
    GtkWidget* label = gtk_label_new("🌐 Peers");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), peersScrolled, label);
}

void MainWindow::createStatsTab() {
    // Create box for statistics
    statsBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(statsBox), 20);
    
    // Create statistics labels
    const char* statLabels[] = {
        "Total Files:",
        "Synced Files:",
        "Active Peers:",
        "Upload Rate:",
        "Download Rate:",
        "Bytes Transferred:",
        "Last Sync:",
        "ML Accuracy:",
        "Anomalies Detected:",
        "Sync Progress:"
    };
    
    for (int i = 0; i < 10; i++) {
        GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_box_pack_start(GTK_BOX(statsBox), hbox, FALSE, FALSE, 0);
        
        GtkWidget* labelName = gtk_label_new(statLabels[i]);
        gtk_label_set_xalign(GTK_LABEL(labelName), 0.0);
        gtk_widget_set_size_request(labelName, 200, -1);
        gtk_box_pack_start(GTK_BOX(hbox), labelName, FALSE, FALSE, 0);
        
        statsLabels[i] = gtk_label_new("N/A");
        gtk_label_set_xalign(GTK_LABEL(statsLabels[i]), 0.0);
        gtk_box_pack_start(GTK_BOX(hbox), statsLabels[i], TRUE, TRUE, 0);
    }
    
    // Progress bar
    progressBar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(statsBox), progressBar, FALSE, FALSE, 10);
    
    // Add to notebook
    GtkWidget* label = gtk_label_new("📊 Statistics");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), statsBox, label);
}

void MainWindow::createLogsTab() {
    // Create scrolled window
    logsScrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(logsScrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // Create text view
    logsTextView = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(logsTextView), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(logsTextView), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(logsTextView), GTK_WRAP_WORD);
    
    // Get text buffer
    logsTextBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(logsTextView));
    
    // Add to scrolled window
    gtk_container_add(GTK_CONTAINER(logsScrolled), logsTextView);
    
    // Add to notebook
    GtkWidget* label = gtk_label_new("📝 Logs");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), logsScrolled, label);
}

void MainWindow::createStatusBar() {
    statusBar = gtk_statusbar_new();
    statusContextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "main");
    gtk_box_pack_end(GTK_BOX(mainBox), statusBar, FALSE, FALSE, 0);
    
    setStatus("Ready");
}

void MainWindow::setupStyles() {
    // Apply custom CSS for modern look
    GtkCssProvider* provider = gtk_css_provider_new();
    const gchar* css = 
        "window { background-color: #f5f5f5; }"
        "notebook { padding: 10px; }"
        "textview { font-family: monospace; font-size: 10pt; }"
        "treeview { font-size: 10pt; }";
    
    gtk_css_provider_load_from_data(provider, css, -1, nullptr);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );
}

void MainWindow::show() {
    gtk_widget_show_all(window);
}

void MainWindow::run() {
    gtk_main();
}

void MainWindow::close() {
    gtk_main_quit();
}

void MainWindow::updateFileList(const std::vector<FileInfo>& files) {
    // Clear existing list
    gtk_list_store_clear(filesListStore);
    
    // Add files
    for (const auto& file : files) {
        GtkTreeIter iter;
        gtk_list_store_append(filesListStore, &iter);
        gtk_list_store_set(filesListStore, &iter,
                          COL_FILE_PATH, file.path.c_str(),
                          COL_FILE_SIZE, formatBytes(file.size).c_str(),
                          COL_FILE_MODIFIED, file.lastModified.c_str(),
                          COL_FILE_STATUS, file.conflictStatus.c_str(),
                          -1);
    }
}

void MainWindow::updatePeerList(const std::vector<PeerInfo>& peers) {
    // Clear existing list
    gtk_list_store_clear(peersListStore);
    
    // Add peers
    for (const auto& peer : peers) {
        GtkTreeIter iter;
        gtk_list_store_append(peersListStore, &iter);
        
        std::string latencyStr = std::to_string(static_cast<int>(peer.latency)) + " ms";
        std::string statusStr = peer.active ? "🟢 Active" : "🔴 Inactive";
        
        gtk_list_store_set(peersListStore, &iter,
                          COL_PEER_ID, peer.id.c_str(),
                          COL_PEER_ADDRESS, peer.address.c_str(),
                          COL_PEER_PORT, peer.port,
                          COL_PEER_LATENCY, latencyStr.c_str(),
                          COL_PEER_STATUS, statusStr.c_str(),
                          -1);
    }
}

void MainWindow::updateStatistics(const GUIStatistics& stats) {
    gtk_label_set_text(GTK_LABEL(statsLabels[0]), std::to_string(stats.totalFiles).c_str());
    gtk_label_set_text(GTK_LABEL(statsLabels[1]), std::to_string(stats.syncedFiles).c_str());
    
    std::string peersText = std::to_string(stats.activePeers) + " / " + std::to_string(stats.totalPeers);
    gtk_label_set_text(GTK_LABEL(statsLabels[2]), peersText.c_str());
    
    gtk_label_set_text(GTK_LABEL(statsLabels[3]), formatRate(stats.uploadRate).c_str());
    gtk_label_set_text(GTK_LABEL(statsLabels[4]), formatRate(stats.downloadRate).c_str());
    gtk_label_set_text(GTK_LABEL(statsLabels[5]), formatBytes(stats.bytesTransferred).c_str());
    gtk_label_set_text(GTK_LABEL(statsLabels[6]), stats.lastSync.c_str());
    
    std::string accuracyText = std::to_string(static_cast<int>(stats.mlAccuracy * 100)) + "%";
    gtk_label_set_text(GTK_LABEL(statsLabels[7]), accuracyText.c_str());
    
    gtk_label_set_text(GTK_LABEL(statsLabels[8]), std::to_string(stats.anomaliesDetected).c_str());
    
    // Update progress bar
    double progress = stats.totalFiles > 0 ? 
                     static_cast<double>(stats.syncedFiles) / stats.totalFiles : 0.0;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressBar), progress);
    
    std::string progressText = std::to_string(static_cast<int>(progress * 100)) + "%";
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressBar), progressText.c_str());
}

void MainWindow::addLogMessage(const std::string& message, const std::string& level) {
    appendLog(message, level);
}

void MainWindow::appendLog(const std::string& message, const std::string& level) {
    // Get current time
    time_t now = time(0);
    struct tm* timeinfo = localtime(&now);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "[%Y-%m-%d %H:%M:%S]", timeinfo);
    
    // Format log message
    std::string logMsg = std::string(timeStr) + " [" + level + "] " + message + "\n";
    
    // Append to text buffer
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(logsTextBuffer, &end);
    gtk_text_buffer_insert(logsTextBuffer, &end, logMsg.c_str(), -1);
    
    // Auto-scroll to bottom
    GtkTextMark* mark = gtk_text_buffer_get_insert(logsTextBuffer);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(logsTextView), mark, 0.0, TRUE, 0.0, 1.0);
}

void MainWindow::setStatus(const std::string& status, bool isError) {
    gtk_statusbar_pop(GTK_STATUSBAR(statusBar), statusContextId);
    
    std::string statusMsg = isError ? "❌ " + status : "✅ " + status;
    gtk_statusbar_push(GTK_STATUSBAR(statusBar), statusContextId, statusMsg.c_str());
}

void MainWindow::showNotification(const std::string& title, const std::string& message) {
    // Create notification dialog
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "%s", title.c_str()
    );
    
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", message.c_str());
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

std::string MainWindow::formatBytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}

std::string MainWindow::formatRate(double mbps) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << mbps << " MB/s";
    return oss.str();
}

// Signal handlers
void MainWindow::onSyncButtonClicked(GtkWidget* widget, gpointer data) {
    MainWindow* window = static_cast<MainWindow*>(data);
    if (window->syncButtonCallback) {
        window->syncButtonCallback();
    }
    window->addLogMessage("Sync triggered manually", "INFO");
}

void MainWindow::onPauseButtonClicked(GtkWidget* widget, gpointer data) {
    MainWindow* window = static_cast<MainWindow*>(data);
    window->addLogMessage("Sync paused", "WARNING");
}

void MainWindow::onSettingsButtonClicked(GtkWidget* widget, gpointer data) {
    MainWindow* window = static_cast<MainWindow*>(data);
    SettingsDialog dialog(GTK_WINDOW(window->window));
    dialog.show();
}

void MainWindow::onAboutButtonClicked(GtkWidget* widget, gpointer data) {
    MainWindow* window = static_cast<MainWindow*>(data);
    showAboutDialog(GTK_WINDOW(window->window));
}

gboolean MainWindow::onWindowDelete(GtkWidget* widget, GdkEvent* event, gpointer data) {
    MainWindow* window = static_cast<MainWindow*>(data);
    window->close();
    return FALSE;
}

void MainWindow::setFileSelectedCallback(FileSelectedCallback callback) {
    fileSelectedCallback = callback;
}

void MainWindow::setSyncButtonCallback(SyncButtonCallback callback) {
    syncButtonCallback = callback;
}

void MainWindow::setSettingsChangedCallback(SettingsChangedCallback callback) {
    settingsChangedCallback = callback;
}

// About dialog
void showAboutDialog(GtkWindow* parent) {
    GtkWidget* dialog = gtk_about_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "SentinelFS-Neo");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.0.0");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
        "Distributed P2P File Synchronization System\n"
        "with Advanced ML Capabilities");
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://github.com/reicalasso/sentinelFS-neo");
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_MIT_X11);
    
    const gchar* authors[] = {"SentinelFS-Neo Team", nullptr};
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), authors);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// Settings Dialog implementation (simplified)
SettingsDialog::SettingsDialog(GtkWindow* parent) {
    dialog = gtk_dialog_new_with_buttons(
        "Settings",
        parent,
        GTK_DIALOG_MODAL,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Save", GTK_RESPONSE_ACCEPT,
        nullptr
    );
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(content), notebook, TRUE, TRUE, 10);
    
    createGeneralTab();
    createSyncTab();
    createMLTab();
}

SettingsDialog::~SettingsDialog() {
    // GTK handles cleanup
}

void SettingsDialog::createGeneralTab() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 20);
    
    // Add basic settings widgets here
    GtkWidget* label = gtk_label_new("General settings coming soon...");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    
    GtkWidget* tabLabel = gtk_label_new("General");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box, tabLabel);
}

void SettingsDialog::createSyncTab() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 20);
    
    GtkWidget* label = gtk_label_new("Sync settings coming soon...");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    
    GtkWidget* tabLabel = gtk_label_new("Sync");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box, tabLabel);
}

void SettingsDialog::createMLTab() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 20);
    
    GtkWidget* label = gtk_label_new("ML settings coming soon...");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    
    GtkWidget* tabLabel = gtk_label_new("Machine Learning");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box, tabLabel);
}

void SettingsDialog::show() {
    gtk_widget_show_all(dialog);
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (result == GTK_RESPONSE_ACCEPT) {
        // Save settings
    }
    
    gtk_widget_destroy(dialog);
}

void SettingsDialog::setConfig(const std::string& key, const std::string& value) {
    // Implementation
}

std::string SettingsDialog::getConfig(const std::string& key) {
    return "";
}
