import SwiftUI
import AppKit

// A menu bar app should not show a Dock icon or a main window. Building as a
// SwiftPM executable (no Info.plist LSUIElement), we set the policy in code.
final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
    }
}

@main
struct DscleanerApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @StateObject private var model = AppModel()

    var body: some Scene {
        // The menu bar item. `.window` style gives us a rich SwiftUI panel
        // instead of a plain NSMenu, which is what makes the modern look possible.
        MenuBarExtra {
            MenuContentView()
                .environmentObject(model)
        } label: {
            Image(systemName: model.status.service.running ? "sparkles" : "sparkle")
                .accessibilityLabel("dscleaner")
        }
        .menuBarExtraStyle(.window)

        // Standard Settings scene (opened from the panel's "Settings…" button).
        Settings {
            SettingsView()
                .environmentObject(model)
                .frame(width: 460)
        }
    }
}
