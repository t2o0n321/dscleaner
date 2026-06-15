import SwiftUI

// Settings window: edit the watched paths and toggle the background service.
// Edits are staged locally and applied with the "Apply" button (which reinstalls
// the launchd agent through the core).
struct SettingsView: View {
    @EnvironmentObject private var model: AppModel
    @State private var paths: [String] = []
    @State private var selection: String?

    var body: some View {
        Form {
            Section("Background service") {
                Toggle("Clean drives automatically on insert", isOn: Binding(
                    get: { model.status.service.running },
                    set: { enabled in
                        Task { await model.setServiceEnabled(enabled, paths: paths) }
                    }
                ))
                .disabled(model.isBusy || !model.coreAvailable)

                LabeledContent("Status") {
                    Text(model.status.service.running ? "Active" : "Paused")
                        .foregroundStyle(model.status.service.running ? .green : .secondary)
                }
            }

            Section("Watched locations") {
                if paths.isEmpty {
                    Text("Defaults to /Volumes (all mounted drives).")
                        .foregroundStyle(.secondary)
                        .font(.callout)
                } else {
                    List(selection: $selection) {
                        ForEach(paths, id: \.self) { path in
                            Label(path, systemImage: "externaldrive")
                                .lineLimit(1)
                                .truncationMode(.middle)
                        }
                    }
                    .frame(minHeight: 90, maxHeight: 160)
                }

                HStack {
                    Button {
                        if let folder = FolderPicker.pickFolder(prompt: "Watch") {
                            if !paths.contains(folder) { paths.append(folder) }
                        }
                    } label: {
                        Label("Add Folder…", systemImage: "plus")
                    }

                    Button {
                        if let selection { paths.removeAll { $0 == selection } }
                    } label: {
                        Label("Remove", systemImage: "minus")
                    }
                    .disabled(selection == nil)

                    Spacer()

                    Button("Apply") {
                        Task { await model.applyWatchPaths(paths) }
                    }
                    .keyboardShortcut(.defaultAction)
                    .disabled(model.isBusy || !model.status.service.running)
                }
            }

            Section("About") {
                LabeledContent("Version", value: model.status.version)
                LabeledContent("Log") {
                    Text(model.status.service.logs.out)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .textSelection(.enabled)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }
            }
        }
        .formStyle(.grouped)
        .frame(width: 460, height: 420)
        .onAppear { paths = model.status.watchPaths }
        .onChange(of: model.status.watchPaths) { newValue in
            // Keep the local editor in sync when the service is (re)configured
            // elsewhere, but don't clobber an in-progress edit.
            if paths.isEmpty { paths = newValue }
        }
    }
}
