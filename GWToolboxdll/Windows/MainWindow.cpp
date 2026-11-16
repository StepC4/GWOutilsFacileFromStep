#include "stdafx.h"

#include <Utils/FontLoader.h>
#include <Utils/GuiUtils.h>
#include <GWToolbox.h>

#include <Windows/MainWindow.h>
#include <Modules/PluginModule.h>


namespace {
    MainWindow::Settings settings;

    constexpr ImVec2 tile_size{96.0f, 72.0f};

    const std::unordered_map<std::string_view, const char*> french_labels{
        {"Travel", "Voyage"},
        {"Builds", "Builds"},
        {"Hero Builds", "Héros"},
        {"Pcons", "Consommables"},
        {"Hotkeys", "Raccourcis"},
        {"Minimap", "Minimap"},
        {"Damage", "Dégâts"},
        {"Bonds", "Liens"},
        {"Health", "Vie cible"},
        {"Timer", "Chrono"},
        {"Objectives", "Objectifs"},
        {"Vanquish", "Vanquish"},
        {"Daily Quests", "Quêtes du jour"},
        {"Materials", "Matériaux"},
        {"Notepad", "Bloc-notes"},
        {"Dialogs", "Dialogues"},
        {"Settings", "Réglages"}
    };

    struct TileCategory {
        const char* title;
        std::vector<std::string_view> module_names;
    };

    const std::vector<TileCategory> tile_categories{
        {"Voyage & Builds", {"Travel", "Builds", "Hero Builds"}},
        {"Combat", {"Pcons", "Hotkeys", "Minimap", "Damage", "Bonds", "Health"}},
        {"Suivi", {"Timer", "Objectives", "Vanquish", "Daily Quests"}},
        {"Outils", {"Materials", "Notepad", "Dialogs", "Settings"}}
    };

    const char* DisplayLabel(const char* name)
    {
        const auto found = french_labels.find(name);
        return found == french_labels.end() ? name : found->second;
    }

    bool DrawTile(const char* icon, const char* label, const bool active)
    {
        const auto& style = ImGui::GetStyle();
        ImGui::PushStyleColor(ImGuiCol_Button, active ? style.Colors[ImGuiCol_ButtonActive] : style.Colors[ImGuiCol_Button]);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::Button("##tile", tile_size);
        ImGui::PopStyleColor();

        ImFont* font = FontLoader::GetFont();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);
        const ImVec4 clip_rect{pos.x, pos.y, pos.x + tile_size.x, pos.y + tile_size.y};
        if (icon) {
            const auto icon_font_size = static_cast<float>(FontLoader::FontSize::widget_label);
            const ImVec2 icon_dim = font->CalcTextSizeA(icon_font_size, FLT_MAX, 0.f, icon);
            draw_list->AddText(font, icon_font_size, ImVec2(pos.x + (tile_size.x - icon_dim.x) / 2, pos.y + tile_size.y * 0.15f), text_color, icon, nullptr, 0.f, &clip_rect);
        }
        const auto label_font_size = static_cast<float>(FontLoader::FontSize::text);
        const ImVec2 label_dim = font->CalcTextSizeA(label_font_size, FLT_MAX, 0.f, label);
        const float label_x = pos.x + std::max(4.0f, (tile_size.x - label_dim.x) / 2);
        draw_list->AddText(font, label_font_size, ImVec2(label_x, pos.y + tile_size.y * 0.62f), text_color, label, nullptr, 0.f, &clip_rect);
        if (ImGui::IsItemHovered() && label_dim.x > tile_size.x - 8.0f) {
            ImGui::SetTooltip("%s", label);
        }
        return clicked;
    }
}

void MainWindow::Initialize()
{
    ToolboxWindow::Initialize();
    SettingsRegistry::Register(this, settings);
}

void MainWindow::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxWindow::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
    show_menubutton = false;
    pending_refresh_buttons = true;
}

void MainWindow::SaveSettings(SettingsDoc& doc)
{
    ToolboxWindow::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void MainWindow::DrawSettingsInternal()
{
    ImGui::Checkbox("Accueil à gros boutons", &settings.big_tiles);
    ImGui::CheckboxWithHelp("Fermer les autres fenêtres à l'ouverture d'une nouvelle", &settings.one_panel_at_time_only, "Concerne seulement les fenêtres (avec barre de titre), pas les widgets");

    if (!settings.big_tiles) {
        ImGui::Checkbox("Afficher les icônes", &settings.show_icons);
        ImGui::Checkbox("Centrer le texte", &settings.center_align_text);
    }
}

void MainWindow::RegisterSettingsContent()
{
    ToolboxModule::RegisterSettingsContent(
        SettingsName(),
        Icon(),
        [this](const std::string&, const bool is_showing) {
            // ShowVisibleRadio();
            if (!is_showing) {
                return;
            }
            ImGui::Text("Visibilité de la fenêtre principale");
            ShowVisibleRadio();
            DrawSizeAndPositionSettings();
            DrawSettingsInternal();
        }, SettingsWeighting());
}

void MainWindow::RefreshButtons()
{
    pending_refresh_buttons = false;
    const std::vector<ToolboxUIElement*>& ui = GWToolbox::GetUIElements();
    modules_to_draw.clear();
    for (auto& ui_module : ui) {
        if (!ui_module->show_menubutton) {
            continue;
        }
        float weighting = GetModuleWeighting(ui_module);
        auto it = modules_to_draw.begin();
        for (it = modules_to_draw.begin(); it != modules_to_draw.end(); ++it) {
            if (it->first > weighting) {
                break;
            }
        }
        modules_to_draw.insert(it, {weighting, ui_module});
    }
}

void MainWindow::OnModuleToggled(const ToolboxUIElement* toggled)
{
    if (!settings.one_panel_at_time_only || !toggled->visible || !toggled->IsWindow()) {
        return;
    }
    for (const auto& module : modules_to_draw | std::views::values) {
        if (module != toggled && module->IsWindow()) {
            module->visible = false;
        }
    }
    for (const auto plugin : PluginModule::GetPlugins() | std::views::filter([](ToolboxPlugin* p) {
        return p && p->GetVisiblePtr() && p->ShowInMainMenu();
    })) {
        *plugin->GetVisiblePtr() = false;
    }
}

void MainWindow::DrawTileGrid()
{
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (tile_size.x + spacing)));

    std::vector<ToolboxUIElement*> remaining;
    for (const auto& module : modules_to_draw | std::views::values) {
        remaining.push_back(module);
    }

    const auto draw_section = [&](const char* title, const std::vector<ToolboxUIElement*>& modules) {
        if (modules.empty()) {
            return;
        }
        ImGui::PushFont(FontLoader::GetFont(), static_cast<float>(FontLoader::FontSize::header2));
        ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_CheckMark], "%s", title);
        ImGui::PopFont();
        ImGui::Separator();
        int column = 0;
        for (const auto module : modules) {
            if (column > 0) {
                ImGui::SameLine();
            }
            ImGui::PushID(module);
            if (DrawTile(module->Icon(), DisplayLabel(module->Name()), module->visible)) {
                module->visible = !module->visible;
                OnModuleToggled(module);
            }
            ImGui::PopID();
            column = (column + 1) % columns;
        }
        ImGui::Spacing();
    };

    for (const auto& category : tile_categories) {
        std::vector<ToolboxUIElement*> in_category;
        for (const auto name : category.module_names) {
            const auto found = std::ranges::find_if(remaining, [name](const ToolboxUIElement* m) { return name == m->Name(); });
            if (found != remaining.end()) {
                in_category.push_back(*found);
                remaining.erase(found);
            }
        }
        draw_section(category.title, in_category);
    }
    draw_section("Autres", remaining);
}

void MainWindow::DrawClassicList()
{
    ImGui::PushFont(FontLoader::GetFont(), static_cast<float>(FontLoader::FontSize::header2));
    bool drawn = false;
    const size_t msize = modules_to_draw.size();
    for (size_t i = 0; i < msize; i++) {
        ImGui::PushID(static_cast<int>(i));
        if (drawn) {
            ImGui::Separator();
        }
        drawn = true;
        const auto& ui_module = modules_to_draw[i].second;
        if (ui_module->DrawTabButton(settings.show_icons, true, settings.center_align_text)) {
            OnModuleToggled(ui_module);
        }
        ImGui::PopID();
    }
    ImGui::PopFont();
}

void MainWindow::Draw(IDirect3DDevice9*)
{
    if (!visible) {
        return;
    }
    if (pending_refresh_buttons) {
        RefreshButtons();
    }
    static bool open = true;
    ImGui::SetNextWindowSize(settings.big_tiles ? ImVec2(440.0f, 460.0f) : ImVec2(110.0f, 300.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), show_closebutton ? &open : nullptr, GetWinFlags())) {
        if (settings.big_tiles) {
            DrawTileGrid();
        }
        else {
            DrawClassicList();
        }
        ImGui::PushFont(FontLoader::GetFont(), static_cast<float>(FontLoader::FontSize::header2));
        size_t i = 0;
        for (const auto plugin : PluginModule::GetPlugins() | std::views::filter([](ToolboxPlugin* p) {
            return p && p->GetVisiblePtr() && p->ShowInMainMenu();
        })) {
            ImGui::PushID(i++);
            ImGui::Separator();
            if (plugin->DrawTabButton(settings.show_icons, true, settings.center_align_text)) {
                if (settings.one_panel_at_time_only && plugin->GetVisiblePtr() && *plugin->GetVisiblePtr()) {
                    for (const auto& module : modules_to_draw | std::views::values) {
                        if (!module->IsWindow()) {
                            continue;
                        }
                        module->visible = false;
                    }
                    for (const auto plug : PluginModule::GetPlugins() | std::views::filter([](ToolboxPlugin* p) {
                        return p && p->GetVisiblePtr() && p->ShowInMainMenu();
                    })) {
                        if (plugin == plug) {
                            continue;
                        }
                        *plug->GetVisiblePtr() = false;
                    }
                }
            }
            ImGui::PopID();
        }
        ImGui::PopFont();
    }
    ImGui::End();

    if (!open) {
        GWToolbox::SignalTerminate();
    }
}
