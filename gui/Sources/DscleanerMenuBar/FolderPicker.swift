import AppKit

// Small wrapper around NSOpenPanel for choosing a directory. Runs modally on the
// main thread and returns the selected path (or nil if cancelled).
enum FolderPicker {
    @MainActor
    static func pickFolder(prompt: String = "Choose") -> String? {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.prompt = prompt
        // Bring the app forward so the panel is not lost behind other windows.
        NSApp.activate(ignoringOtherApps: true)
        guard panel.runModal() == .OK else { return nil }
        return panel.url?.path
    }
}
