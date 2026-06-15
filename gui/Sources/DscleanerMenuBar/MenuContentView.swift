import SwiftUI
import AppKit

// Overall health shown by the status pill.
private enum ServiceState {
    case running, stopped, unavailable
}

// The panel shown when the user clicks the menu bar icon. Status at a glance,
// plus the common one-click actions. Settings live in a separate window.
struct MenuContentView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            header
            Divider()
            content
            Divider()
            footer
        }
        .frame(width: 320)
    }

    // MARK: - Header

    private var header: some View {
        HStack(spacing: 10) {
            Image(systemName: "sparkles")
                .font(.title2)
                .foregroundStyle(.tint)
            VStack(alignment: .leading, spacing: 1) {
                Text("dscleaner")
                    .font(.headline)
                Text("macOS junk remover")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            Spacer()
            StatusPill(state: state)
        }
        .padding(12)
    }

    // MARK: - Content

    private var content: some View {
        VStack(alignment: .leading, spacing: 12) {
            if !model.coreAvailable {
                Label(model.lastError ?? "The dscleaner command was not found.",
                      systemImage: "exclamationmark.triangle.fill")
                    .font(.callout)
                    .foregroundStyle(.orange)
            } else {
                serviceRow
                if !model.status.watchPaths.isEmpty {
                    watchedPathsSection
                }
                if let error = model.lastError {
                    Text(error)
                        .font(.caption)
                        .foregroundStyle(.red)
                        .lineLimit(2)
                }
            }
        }
        .padding(12)
    }

    private var serviceRow: some View {
        Toggle(isOn: Binding(
            get: { model.status.service.running },
            set: { enabled in
                Task { await model.setServiceEnabled(enabled, paths: model.status.watchPaths) }
            }
        )) {
            VStack(alignment: .leading, spacing: 1) {
                Text("Clean drives automatically")
                Text("Watches mounted volumes and cleans on insert")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
        .toggleStyle(.switch)
        .disabled(model.isBusy)
    }

    private var watchedPathsSection: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("WATCHING")
                .font(.caption2.weight(.semibold))
                .foregroundStyle(.secondary)
            ForEach(model.status.watchPaths, id: \.self) { path in
                Label(path, systemImage: "externaldrive")
                    .font(.callout)
                    .lineLimit(1)
                    .truncationMode(.middle)
            }
        }
    }

    // MARK: - Footer

    private var footer: some View {
        VStack(spacing: 6) {
            Button {
                Task {
                    if let folder = FolderPicker.pickFolder(prompt: "Clean") {
                        await model.clean(path: folder)
                    }
                }
            } label: {
                Label("Clean a Folder…", systemImage: "wand.and.sparkles")
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            .buttonStyle(.plain)

            HStack {
                Button("Settings…") { openSettings() }
                Spacer()
                Button("Quit") { NSApplication.shared.terminate(nil) }
            }
            .buttonStyle(.plain)
            .foregroundStyle(.secondary)
            .font(.callout)
        }
        .padding(12)
    }

    // MARK: - Helpers

    private var state: ServiceState {
        if !model.coreAvailable { return .unavailable }
        return model.status.service.running ? .running : .stopped
    }

    private func openSettings() {
        // Selector differs between macOS versions; try the modern one first.
        if !NSApp.sendAction(Selector(("showSettingsWindow:")), to: nil, from: nil) {
            NSApp.sendAction(Selector(("showPreferencesWindow:")), to: nil, from: nil)
        }
        NSApp.activate(ignoringOtherApps: true)
    }
}

private struct StatusPill: View {
    let state: ServiceState

    private var text: String {
        switch state {
        case .running: return "Active"
        case .stopped: return "Paused"
        case .unavailable: return "Unavailable"
        }
    }

    private var color: Color {
        switch state {
        case .running: return .green
        case .stopped: return .orange
        case .unavailable: return .red
        }
    }

    var body: some View {
        HStack(spacing: 5) {
            Circle().fill(color).frame(width: 7, height: 7)
            Text(text).font(.caption.weight(.medium))
        }
        .padding(.horizontal, 9)
        .padding(.vertical, 4)
        .background(color.opacity(0.15), in: Capsule())
        .foregroundStyle(color)
    }
}
