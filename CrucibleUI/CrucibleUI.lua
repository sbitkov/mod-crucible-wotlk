local ADDON_PREFIX = "CRUCIBLE"

local selected = nil
local pending = nil
local internalPickup = false
local absorbPending = false
local pendingAbsorbGuid = nil

local previewGuid = nil
local previewResult = nil
local previewStats = {}

local statsReceiving = false
local statsRows = {}

local essencesReceiving = false
local essenceRows = {}
local essenceFilterMastery = 0
local essenceSearchText = ""
local essenceDisplayRows = {}

local selectedEssenceEntry = nil
local masteryPreviewEntry = nil
local masteryPreviewResult = nil
local masteryCurrent = nil
local masteryNext = nil
local masteryMoney = 0
local masteryReagentEntry = 0
local masteryReagentCount = 0
local masteryStats = {}
local masteryUpgradePending = false

local function MakeBagLocation(bag, slot)
    return { bagID = bag, slotIndex = slot }
end

local function GetGuidAt(bag, slot)
    if not C_Item or not C_Item.GetItemGUID then
        return nil
    end
    return C_Item.GetItemGUID(MakeBagLocation(bag, slot))
end

local function GetCurrentLocation(guid)
    if not guid or not C_Item or not C_Item.GetItemLocation then
        return nil
    end
    return C_Item.GetItemLocation(guid)
end

local function SplitTabs(text)
    local parts = {}
    local start = 1

    while true do
        local pos = string.find(text, "\t", start, true)
        if not pos then
            table.insert(parts, string.sub(text, start))
            break
        end

        table.insert(parts, string.sub(text, start, pos - 1))
        start = pos + 1
    end

    return parts
end

local function CapturePickup(bag, slot)
    if internalPickup then
        return
    end

    if not CursorHasItem or not CursorHasItem() then
        return
    end

    pending = {
        bag = bag,
        slot = slot,
        guid = GetGuidAt(bag, slot),
        link = GetContainerItemLink(bag, slot),
    }
end

hooksecurefunc("PickupContainerItem", CapturePickup)

local frame = CreateFrame("Frame", "CrucibleFrame", UIParent)
frame:SetWidth(720)
frame:SetHeight(380)
frame:SetPoint("CENTER", UIParent, "CENTER", 0, 30)
frame:SetFrameStrata("DIALOG")
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", function(self) self:StartMoving() end)
frame:SetScript("OnDragStop", function(self) self:StopMovingOrSizing() end)

frame:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true,
    tileSize = 32,
    edgeSize = 32,
    insets = { left = 11, right = 12, top = 12, bottom = 11 }
})

local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
title:SetPoint("TOP", frame, "TOP", 0, -18)
title:SetText("Crucible")

local closeButton = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
closeButton:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -5, -5)

local instruction = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
instruction:SetPoint("TOP", frame, "TOP", 150, -52)
instruction:SetText("Drag an item here")

local slot = CreateFrame("Button", "CrucibleItemSlot", frame)
slot:SetWidth(64)
slot:SetHeight(64)
slot:SetPoint("TOP", instruction, "BOTTOM", 0, -14)
slot:RegisterForClicks("LeftButtonUp", "RightButtonUp")
slot:RegisterForDrag("LeftButton")
slot:SetNormalTexture("Interface\\Buttons\\UI-Quickslot2")
slot:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")

local icon = slot:CreateTexture(nil, "BACKGROUND")
icon:SetPoint("TOPLEFT", slot, "TOPLEFT", 5, -5)
icon:SetPoint("BOTTOMRIGHT", slot, "BOTTOMRIGHT", -5, 5)
icon:Hide()

local nameText = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
nameText:SetPoint("TOP", slot, "BOTTOM", 0, -10)
nameText:SetWidth(350)
nameText:SetText("No item selected")

local previewTitle = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
previewTitle:SetPoint("TOPLEFT", frame, "TOPLEFT", 342, -192)
previewTitle:SetText("If absorbed:")

local previewText = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
previewText:SetPoint("TOPLEFT", previewTitle, "BOTTOMLEFT", 0, -8)
previewText:SetWidth(336)
previewText:SetJustifyH("LEFT")
previewText:SetJustifyV("TOP")
previewText:SetText("")

local statusText = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
statusText:SetPoint("BOTTOM", frame, "BOTTOM", 150, 56)
statusText:SetWidth(350)
statusText:SetText("")

local progressionButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
progressionButton:SetWidth(120)
progressionButton:SetHeight(24)
progressionButton:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 362, 22)
progressionButton:SetText("Progression")

local absorbButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
absorbButton:SetWidth(110)
absorbButton:SetHeight(24)
absorbButton:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -62, 22)
absorbButton:SetText("Absorb")
absorbButton:Disable()

local essencePanel = CreateFrame("Frame", nil, frame)
essencePanel:SetPoint("TOPLEFT", frame, "TOPLEFT", 20, -48)
essencePanel:SetPoint("BOTTOMRIGHT", frame, "BOTTOMLEFT", 310, 20)

local essenceTitle = essencePanel:CreateFontString(nil, "OVERLAY", "GameFontNormal")
essenceTitle:SetPoint("TOPLEFT", essencePanel, "TOPLEFT", 8, 0)
essenceTitle:SetText("Stored Essences")

local essenceSearch = CreateFrame("EditBox", "CrucibleEssenceSearchBox", essencePanel, "InputBoxTemplate")
essenceSearch:SetWidth(245)
essenceSearch:SetHeight(22)
essenceSearch:SetPoint("TOPLEFT", essenceTitle, "BOTTOMLEFT", 0, -8)
essenceSearch:SetAutoFocus(false)
essenceSearch:SetMaxLetters(64)

local essenceSearchHint = essencePanel:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
essenceSearchHint:SetPoint("LEFT", essenceSearch, "LEFT", 6, 0)
essenceSearchHint:SetText("Search...")

local essenceFilterButtons = {}
local essenceFilterValues = { 0, 20, 40, 60, 80, 100 }
local essenceFilterLabels = { "All", "20", "40", "60", "80", "100" }

local essenceScroll = CreateFrame("ScrollFrame", "CrucibleEssenceScrollFrame", essencePanel, "UIPanelScrollFrameTemplate")
essenceScroll:SetPoint("TOPLEFT", essencePanel, "TOPLEFT", 4, -82)
essenceScroll:SetPoint("BOTTOMRIGHT", essencePanel, "BOTTOMRIGHT", -28, 28)

local essenceContent = CreateFrame("Frame", nil, essenceScroll)
essenceContent:SetWidth(245)
essenceContent:SetHeight(1)
essenceScroll:SetScrollChild(essenceContent)

local essenceEmptyText = essenceContent:CreateFontString(nil, "OVERLAY", "GameFontDisable")
essenceEmptyText:SetPoint("TOP", essenceContent, "TOP", 0, -18)
essenceEmptyText:SetWidth(230)
essenceEmptyText:SetText("No stored essences.")
essenceEmptyText:Hide()

local essenceStatus = essencePanel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
essenceStatus:SetPoint("BOTTOMLEFT", essencePanel, "BOTTOMLEFT", 8, 2)
essenceStatus:SetWidth(235)
essenceStatus:SetJustifyH("LEFT")
essenceStatus:SetText("")

local function ClearEssenceDisplayRows()
    for _, row in ipairs(essenceDisplayRows) do
        row:Hide()
        row:SetParent(nil)
    end
    essenceDisplayRows = {}
end

local function GetEssenceItemInfo(itemEntry)
    local name, link, _, _, _, _, _, _, _, texture = GetItemInfo(itemEntry)
    if not name then
        name = "Item " .. tostring(itemEntry)
    end
    if not texture and GetItemIcon then
        texture = GetItemIcon(itemEntry)
    end
    return name, link, texture
end

local function EssencePassesFilter(row)
    if essenceFilterMastery ~= 0 and row.mastery ~= essenceFilterMastery then
        return false
    end

    if essenceSearchText == "" then
        return true
    end

    local name = string.lower(row.name or ("Item " .. tostring(row.entry)))
    return string.find(name, essenceSearchText, 1, true) ~= nil
end

local OpenMasteryForEssence

local function RenderEssences()
    ClearEssenceDisplayRows()
    essenceEmptyText:Hide()

    local visible = {}

    for _, row in ipairs(essenceRows) do
        local name, link, texture = GetEssenceItemInfo(row.entry)
        row.name = name
        row.link = link
        row.texture = texture

        if EssencePassesFilter(row) then
            table.insert(visible, row)
        end
    end

    table.sort(visible, function(a, b)
        local an = string.lower(a.name or "")
        local bn = string.lower(b.name or "")
        if an == bn then
            return a.entry < b.entry
        end
        return an < bn
    end)

    if #visible == 0 then
        if #essenceRows == 0 then
            essenceEmptyText:SetText("No stored essences.")
        else
            essenceEmptyText:SetText("No essences match the filter.")
        end
        essenceEmptyText:Show()
        essenceContent:SetHeight(80)
    else
        local rowHeight = 34

        for index, essence in ipairs(visible) do
            local row = CreateFrame("Button", nil, essenceContent)
            row:SetWidth(240)
            row:SetHeight(rowHeight)
            row:SetPoint("TOPLEFT", essenceContent, "TOPLEFT", 0, -((index - 1) * rowHeight))
            row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight", "ADD")

            local rowIcon = row:CreateTexture(nil, "ARTWORK")
            rowIcon:SetWidth(28)
            rowIcon:SetHeight(28)
            rowIcon:SetPoint("LEFT", row, "LEFT", 2, 0)
            rowIcon:SetTexture(essence.texture or "Interface\\Icons\\INV_Misc_QuestionMark")

            local rowName = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
            rowName:SetPoint("LEFT", rowIcon, "RIGHT", 6, 6)
            rowName:SetWidth(155)
            rowName:SetJustifyH("LEFT")
            rowName:SetText(essence.name)

            local rowEntry = row:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
            rowEntry:SetPoint("LEFT", rowIcon, "RIGHT", 6, -7)
            rowEntry:SetWidth(155)
            rowEntry:SetJustifyH("LEFT")
            rowEntry:SetText("ID " .. tostring(essence.entry))

            local rowMastery = row:CreateFontString(nil, "OVERLAY", "GameFontNormal")
            rowMastery:SetPoint("RIGHT", row, "RIGHT", -4, 0)
            rowMastery:SetWidth(45)
            rowMastery:SetJustifyH("RIGHT")
            rowMastery:SetText(tostring(essence.mastery) .. "%")

            row:SetScript("OnEnter", function(self)
                GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
                if essence.link then
                    GameTooltip:SetHyperlink(essence.link)
                else
                    GameTooltip:SetText(essence.name or ("Item " .. tostring(essence.entry)))
                    GameTooltip:AddLine("Mastery: " .. tostring(essence.mastery) .. "%", 1, 1, 1)
                    GameTooltip:Show()
                end
            end)

            row:SetScript("OnLeave", function()
                GameTooltip:Hide()
            end)

            row:SetScript("OnClick", function()
                OpenMasteryForEssence(essence.entry)
            end)

            table.insert(essenceDisplayRows, row)
        end

        essenceContent:SetHeight(math.max(1, #visible * rowHeight))
    end

    essenceStatus:SetText(tostring(#visible) .. " shown / " .. tostring(#essenceRows) .. " stored")
end

for index, mastery in ipairs(essenceFilterValues) do
    local button = CreateFrame("Button", nil, essencePanel, "UIPanelButtonTemplate")
    button:SetWidth(index == 1 and 42 or 36)
    button:SetHeight(20)

    if index == 1 then
        button:SetPoint("TOPLEFT", essenceSearch, "BOTTOMLEFT", 0, -7)
    else
        button:SetPoint("LEFT", essenceFilterButtons[index - 1], "RIGHT", 2, 0)
    end

    button:SetText(essenceFilterLabels[index])
    button:SetScript("OnClick", function()
        essenceFilterMastery = mastery
        RenderEssences()
    end)

    essenceFilterButtons[index] = button
end

essenceSearch:SetScript("OnTextChanged", function(self)
    local text = self:GetText() or ""
    essenceSearchText = string.lower(text)
    if text == "" then
        essenceSearchHint:Show()
    else
        essenceSearchHint:Hide()
    end
    RenderEssences()
end)

essenceSearch:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
essenceSearch:SetScript("OnEnterPressed", function(self) self:ClearFocus() end)

local function RequestEssences()
    essencesReceiving = false
    essenceRows = {}
    essenceStatus:SetText("Requesting essences...")

    SendAddonMessage(
        ADDON_PREFIX,
        "ESSENCES",
        "WHISPER",
        UnitName("player")
    )
end

local masteryPanel = CreateFrame("Frame", nil, frame)
masteryPanel:SetPoint("TOPLEFT", frame, "TOPLEFT", 326, -48)
masteryPanel:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -22, 20)
masteryPanel:Hide()

local masteryTitle = masteryPanel:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
masteryTitle:SetPoint("TOP", masteryPanel, "TOP", 0, -4)
masteryTitle:SetText("Essence Mastery")

local masteryIcon = masteryPanel:CreateTexture(nil, "ARTWORK")
masteryIcon:SetWidth(48)
masteryIcon:SetHeight(48)
masteryIcon:SetPoint("TOPLEFT", masteryPanel, "TOPLEFT", 22, -48)

local masteryName = masteryPanel:CreateFontString(nil, "OVERLAY", "GameFontNormal")
masteryName:SetPoint("LEFT", masteryIcon, "RIGHT", 10, 8)
masteryName:SetWidth(285)
masteryName:SetJustifyH("LEFT")
masteryName:SetText("")

local masteryTierText = masteryPanel:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
masteryTierText:SetPoint("LEFT", masteryIcon, "RIGHT", 10, -12)
masteryTierText:SetWidth(285)
masteryTierText:SetJustifyH("LEFT")
masteryTierText:SetText("")

local masteryGainTitle = masteryPanel:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
masteryGainTitle:SetPoint("TOPLEFT", masteryPanel, "TOPLEFT", 22, -118)
masteryGainTitle:SetText("Next mastery gain:")

local masteryGainText = masteryPanel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
masteryGainText:SetPoint("TOPLEFT", masteryGainTitle, "BOTTOMLEFT", 0, -8)
masteryGainText:SetWidth(335)
masteryGainText:SetJustifyH("LEFT")
masteryGainText:SetJustifyV("TOP")
masteryGainText:SetText("")

local masteryCostTitle = masteryPanel:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
masteryCostTitle:SetPoint("TOPLEFT", masteryPanel, "TOPLEFT", 22, -228)
masteryCostTitle:SetText("Upgrade cost:")

local masteryCostText = masteryPanel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
masteryCostText:SetPoint("TOPLEFT", masteryCostTitle, "BOTTOMLEFT", 0, -8)
masteryCostText:SetWidth(335)
masteryCostText:SetJustifyH("LEFT")
masteryCostText:SetText("")

local masteryStatus = masteryPanel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
masteryStatus:SetPoint("BOTTOM", masteryPanel, "BOTTOM", 0, 58)
masteryStatus:SetWidth(335)
masteryStatus:SetText("")

local masteryBackButton = CreateFrame("Button", nil, masteryPanel, "UIPanelButtonTemplate")
masteryBackButton:SetWidth(110)
masteryBackButton:SetHeight(24)
masteryBackButton:SetPoint("BOTTOMLEFT", masteryPanel, "BOTTOMLEFT", 38, 20)
masteryBackButton:SetText("Back to Absorb")

local masteryUpgradeButton = CreateFrame("Button", nil, masteryPanel, "UIPanelButtonTemplate")
masteryUpgradeButton:SetWidth(110)
masteryUpgradeButton:SetHeight(24)
masteryUpgradeButton:SetPoint("BOTTOMRIGHT", masteryPanel, "BOTTOMRIGHT", -38, 20)
masteryUpgradeButton:SetText("Upgrade")
masteryUpgradeButton:Disable()

local function SetAbsorbModeVisible(visible)
    local widgets = {
        instruction, slot, nameText, previewTitle, previewText,
        statusText, progressionButton, absorbButton
    }

    for _, widget in ipairs(widgets) do
        if visible then
            widget:Show()
        else
            widget:Hide()
        end
    end
end

local function FormatMasteryValue(value)
    local n = tonumber(value)
    if not n then
        return tostring(value)
    end

    local text = string.format("%.4f", n)
    text = string.gsub(text, "0+$", "")
    text = string.gsub(text, "%.$", "")

    if n > 0 then
        return "+" .. text
    end

    return text
end
local function FormatMoneyCopper(copper)
    local value = tonumber(copper) or 0
    local gold = math.floor(value / 10000)
    local silver = math.floor((value % 10000) / 100)
    local copperPart = value % 100

    local parts = {}
    if gold > 0 then table.insert(parts, tostring(gold) .. "g") end
    if silver > 0 then table.insert(parts, tostring(silver) .. "s") end
    if copperPart > 0 or #parts == 0 then table.insert(parts, tostring(copperPart) .. "c") end

    return table.concat(parts, " ")
end

local function ResetMasteryPreview()
    masteryPreviewEntry = nil
    masteryPreviewResult = nil
    masteryCurrent = nil
    masteryNext = nil
    masteryMoney = 0
    masteryReagentEntry = 0
    masteryReagentCount = 0
    masteryStats = {}
    masteryUpgradePending = false
    masteryGainText:SetText("")
    masteryCostText:SetText("")
    masteryStatus:SetText("")
    masteryUpgradeButton:SetText("Upgrade")
    masteryUpgradeButton:Disable()
end

local function RenderMasteryPreview()
    if not selectedEssenceEntry or masteryPreviewEntry ~= selectedEssenceEntry then
        return
    end

    local name, link, texture = GetEssenceItemInfo(selectedEssenceEntry)
    masteryName:SetText(link or name)
    masteryIcon:SetTexture(texture or "Interface\\Icons\\INV_Misc_QuestionMark")

    if masteryPreviewResult == "MAX_MASTERY" then
        masteryTierText:SetText("Mastery: 100%")
        masteryGainText:SetText("Maximum mastery reached.")
        masteryCostText:SetText("No further upgrade available.")
        masteryStatus:SetText("")
        masteryUpgradeButton:Disable()
        return
    end

    if masteryPreviewResult == "UNSUPPORTED_BRACKET" then
        masteryTierText:SetText("Mastery: " .. tostring(masteryCurrent or "?") .. "%")
        masteryGainText:SetText("")
        masteryCostText:SetText("Upgrade economy is not configured for this bracket yet.")
        masteryStatus:SetText("")
        masteryUpgradeButton:Disable()
        return
    end

    if masteryPreviewResult ~= "SUCCESS" then
        masteryTierText:SetText("")
        masteryGainText:SetText("")
        masteryCostText:SetText("")
        masteryStatus:SetText("Unavailable: " .. tostring(masteryPreviewResult))
        masteryUpgradeButton:Disable()
        return
    end

    masteryTierText:SetText(
        "Mastery: " .. tostring(masteryCurrent) .. "%  ->  " .. tostring(masteryNext) .. "%"
    )

    local gainLines = {}
    for _, stat in ipairs(masteryStats) do
        table.insert(gainLines, FormatMasteryValue(stat.value) .. " " .. stat.name)
    end
    masteryGainText:SetText(table.concat(gainLines, "\n"))

    local costLines = { FormatMoneyCopper(masteryMoney) }
    if masteryReagentEntry ~= 0 and masteryReagentCount > 0 then
        local reagentName, reagentLink = GetItemInfo(masteryReagentEntry)
        table.insert(
            costLines,
            tostring(masteryReagentCount) .. "x " ..
            (reagentLink or reagentName or ("Item " .. tostring(masteryReagentEntry)))
        )
    end
    masteryCostText:SetText(table.concat(costLines, "\n"))

    masteryStatus:SetText("Ready")
    if masteryUpgradePending then
        masteryUpgradeButton:Disable()
    else
        masteryUpgradeButton:Enable()
    end
end

local function RequestMasteryPreview(itemEntry)
    ResetMasteryPreview()
    masteryPreviewEntry = itemEntry
    masteryStatus:SetText("Loading mastery...")

    SendAddonMessage(
        ADDON_PREFIX,
        "MASTERY_PREVIEW\t" .. tostring(itemEntry),
        "WHISPER",
        UnitName("player")
    )
end

OpenMasteryForEssence = function(itemEntry)
    selectedEssenceEntry = itemEntry
    SetAbsorbModeVisible(false)
    masteryPanel:Show()
    RequestMasteryPreview(itemEntry)
end

masteryBackButton:SetScript("OnClick", function()
    selectedEssenceEntry = nil
    masteryPanel:Hide()
    ResetMasteryPreview()
    SetAbsorbModeVisible(true)
end)

masteryUpgradeButton:SetScript("OnClick", function()
    if masteryUpgradePending or not selectedEssenceEntry then
        return
    end

    masteryUpgradePending = true
    masteryUpgradeButton:Disable()
    masteryUpgradeButton:SetText("Upgrading...")
    masteryStatus:SetText("Waiting for server...")

    SendAddonMessage(
        ADDON_PREFIX,
        "MASTERY_UPGRADE\t" .. tostring(selectedEssenceEntry),
        "WHISPER",
        UnitName("player")
    )
end)

local function RestoreReservedVisuals()
    if not NUM_CONTAINER_FRAMES then
        return
    end

    for frameIndex = 1, NUM_CONTAINER_FRAMES do
        local bagFrame = _G["ContainerFrame" .. frameIndex]
        if bagFrame and bagFrame.size then
            local frameName = bagFrame:GetName()

            for i = 1, bagFrame.size do
                local button = _G[frameName .. "Item" .. i]
                if button and button.crucibleReserved then
                    local bag = bagFrame:GetID()
                    local slotIndex = button:GetID()
                    local _, _, locked = GetContainerItemInfo(bag, slotIndex)

                    SetItemButtonDesaturated(button, locked and true or false, 0.5, 0.5, 0.5)
                    button.crucibleReserved = nil
                end
            end
        end
    end
end

local function FindVisibleBagButton(bag, slotIndex)
    if not NUM_CONTAINER_FRAMES then
        return nil
    end

    for frameIndex = 1, NUM_CONTAINER_FRAMES do
        local bagFrame = _G["ContainerFrame" .. frameIndex]

        if bagFrame and bagFrame:IsShown() and bagFrame:GetID() == bag and bagFrame.size then
            local frameName = bagFrame:GetName()

            for i = 1, bagFrame.size do
                local button = _G[frameName .. "Item" .. i]
                if button and button:GetID() == slotIndex then
                    return button
                end
            end
        end
    end

    return nil
end

local function ApplyReservationVisual()
    RestoreReservedVisuals()

    if not selected or not selected.guid or absorbPending then
        return
    end

    local location = GetCurrentLocation(selected.guid)
    if not location or location.bagID == nil or location.slotIndex == nil then
        return
    end

    selected.currentLocation = location

    local button = FindVisibleBagButton(location.bagID, location.slotIndex)
    if not button then
        return
    end

    button.crucibleReserved = true
    SetItemButtonDesaturated(button, true, 0.5, 0.5, 0.5)
end

local function ResetPreview()
    previewGuid = nil
    previewResult = nil
    previewStats = {}
    previewText:SetText("")
end

local function ResetSlot(status)
    RestoreReservedVisuals()
    ResetPreview()

    selected = nil
    pending = nil
    absorbPending = false
    pendingAbsorbGuid = nil

    icon:SetTexture(nil)
    icon:Hide()
    nameText:SetText("No item selected")
    statusText:SetText(status or "")
    absorbButton:SetText("Absorb")
    absorbButton:Disable()
end

local function RefreshSelectedLocation()
    if not selected or not selected.guid then
        return false
    end

    local location = GetCurrentLocation(selected.guid)
    if not location then
        return false
    end

    selected.currentLocation = location
    return true
end

local PREVIEW_ERROR_TEXT = {
    ALREADY_ABSORBED = "Already absorbed",
    QUALITY_TOO_LOW = "Only Uncommon or better equipment can be absorbed",
    ITEM_NOT_EQUIPMENT = "This item is not eligible equipment",
    ITEM_NOT_USABLE = "This character cannot use this item",
    ARMOR_TYPE_NOT_ALLOWED = "Armor type is outside this class gear-space",
    WEAPON_TYPE_NOT_ALLOWED = "Weapon proficiency is not available",
    WEAPON_UNSUPPORTED = "This weapon type is not supported yet",
    ITEM_IN_TRADE = "Item is in trade",
    NO_SUPPORTED_STATS = "No supported stats",
    ITEM_TEMPLATE_NOT_FOUND = "Item template not found",
    INVALID_ARGUMENT = "Invalid request",
    ITEM_NOT_FOUND = "Item not found",
}

local function FormatAbsorbedValue(value)
    local n = tonumber(value)
    if not n then
        return tostring(value)
    end

    local text = string.format("%.4f", n)
    text = string.gsub(text, "0+$", "")
    text = string.gsub(text, "%.$", "")

    if n > 0 then
        return "+" .. text
    end

    return text
end

local function RenderPreview()
    if not selected or not previewGuid or previewGuid ~= selected.guid then
        return
    end

    if previewResult ~= "SUCCESS" then
        previewText:SetText(PREVIEW_ERROR_TEXT[previewResult] or ("Unavailable: " .. tostring(previewResult)))
        statusText:SetText("")
        absorbButton:Disable()
        return
    end

    local lines = {}

    for _, stat in ipairs(previewStats) do
        table.insert(lines, FormatMasteryValue(stat.value) .. " " .. stat.name)
    end

    previewText:SetText(table.concat(lines, "\n"))
    statusText:SetText("Ready")

    if #previewStats > 0 then
        absorbButton:Enable()
    else
        absorbButton:Disable()
    end
end

local function RequestPreview()
    if not selected or not selected.guid then
        return
    end

    ResetPreview()
    previewGuid = selected.guid
    statusText:SetText("Calculating...")
    previewText:SetText("")
    absorbButton:Disable()

    SendAddonMessage(
        ADDON_PREFIX,
        "PREVIEW\t" .. selected.guid,
        "WHISPER",
        UnitName("player")
    )
end

local function ResolveItemTexture(info)
    if not info then
        return nil
    end

    local texture = nil

    if GetItemIcon and info.link then
        texture = GetItemIcon(info.link)
    end

    if not texture and info.link then
        texture = select(10, GetItemInfo(info.link))
    end

    if not texture and info.bag ~= nil and info.slot ~= nil then
        texture = GetContainerItemInfo(info.bag, info.slot)
    end

    return texture
end

local function SetSelection(info)
    RestoreReservedVisuals()
    ResetPreview()

    selected = info
    absorbPending = false
    pendingAbsorbGuid = nil

    info.texture = ResolveItemTexture(info)

    if info.texture then
        icon:SetTexture(info.texture)
        icon:Show()
    else
        icon:SetTexture(nil)
        icon:Hide()
    end

    nameText:SetText(info.link or "Selected item")

    if RefreshSelectedLocation() and info.guid then
        ApplyReservationVisual()
        RequestPreview()
    else
        statusText:SetText("Item no longer available")
        absorbButton:Disable()
    end
end

local function ReturnCursorToSource(info)
    if not CursorHasItem or not CursorHasItem() then
        return true
    end

    internalPickup = true
    PickupContainerItem(info.bag, info.slot)
    internalPickup = false

    return not CursorHasItem()
end

local function AcceptCursorItem()
    if not CursorHasItem or not CursorHasItem() then
        return
    end

    if not pending or not pending.guid then
        statusText:SetText("Could not identify item")
        return
    end

    local info = pending
    pending = nil

    if not ReturnCursorToSource(info) then
        statusText:SetText("Could not return item")
        return
    end

    info.link = GetContainerItemLink(info.bag, info.slot) or info.link
    SetSelection(info)
end

local RESULT_TEXT = {
    SUCCESS = "Absorbed",
    ALREADY_ABSORBED = "Already absorbed",
    WEAPON_UNSUPPORTED = "Weapons are not supported",
    ITEM_IN_TRADE = "Item is in trade",
    NO_SUPPORTED_STATS = "No supported stats",
    ITEM_TEMPLATE_NOT_FOUND = "Item template not found",
    INVALID_ARGUMENT = "Invalid request",
    DESTROY_FAILED = "Could not destroy item",
    ITEM_NOT_FOUND = "Item not found",
    UNKNOWN = "Unknown server result",
}

local function HandleResult(resultName, guid)
    if not absorbPending then
        return
    end

    if not pendingAbsorbGuid or guid ~= pendingAbsorbGuid then
        return
    end

    ResetSlot(RESULT_TEXT[resultName] or ("Server result: " .. tostring(resultName)))
end

slot:SetScript("OnReceiveDrag", AcceptCursorItem)

slot:SetScript("OnClick", function(self, button)
    if CursorHasItem and CursorHasItem() then
        AcceptCursorItem()
        return
    end

    if button == "RightButton" and selected and not absorbPending then
        ResetSlot("")
    end
end)

slot:SetScript("OnEnter", function(self)
    if selected and selected.link then
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetHyperlink(selected.link)
        GameTooltip:Show()
    end
end)

slot:SetScript("OnLeave", function()
    GameTooltip:Hide()
end)

absorbButton:SetScript("OnClick", function()
    if absorbPending or not selected or not selected.guid then
        return
    end

    if previewResult ~= "SUCCESS" then
        return
    end

    if not RefreshSelectedLocation() then
        ResetSlot("Item no longer available")
        return
    end

    local guid = selected.guid

    absorbPending = true
    pendingAbsorbGuid = guid
    RestoreReservedVisuals()

    absorbButton:Disable()
    absorbButton:SetText("Sending...")
    statusText:SetText("Waiting for server")

    SendAddonMessage(
        ADDON_PREFIX,
        "ABSORB\t" .. guid,
        "WHISPER",
        UnitName("player")
    )
end)

frame:SetScript("OnShow", function()
    ResetSlot("")
    RequestEssences()
end)

frame:SetScript("OnHide", function()
    RestoreReservedVisuals()
    pending = nil
    selected = nil
    absorbPending = false
    pendingAbsorbGuid = nil
    ResetPreview()
end)

frame:Hide()

local progressionFrame = CreateFrame("Frame", "CrucibleProgressionFrame", UIParent)
progressionFrame:SetWidth(420)
progressionFrame:SetHeight(430)
progressionFrame:SetPoint("CENTER", UIParent, "CENTER", 460, 30)
progressionFrame:SetFrameStrata("DIALOG")
progressionFrame:SetMovable(true)
progressionFrame:EnableMouse(true)
progressionFrame:RegisterForDrag("LeftButton")
progressionFrame:SetScript("OnDragStart", function(self) self:StartMoving() end)
progressionFrame:SetScript("OnDragStop", function(self) self:StopMovingOrSizing() end)

progressionFrame:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true,
    tileSize = 32,
    edgeSize = 32,
    insets = { left = 11, right = 12, top = 12, bottom = 11 }
})

local progressionTitle = progressionFrame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
progressionTitle:SetPoint("TOP", progressionFrame, "TOP", 0, -18)
progressionTitle:SetText("Crucible Progression")

local progressionCloseButton = CreateFrame("Button", nil, progressionFrame, "UIPanelCloseButton")
progressionCloseButton:SetPoint("TOPRIGHT", progressionFrame, "TOPRIGHT", -5, -5)

local progressionSubtitle = progressionFrame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
progressionSubtitle:SetPoint("TOP", progressionTitle, "BOTTOM", 0, -8)
progressionSubtitle:SetText("Permanent bonuses accumulated from absorbed equipment")

local progressionHeader = progressionFrame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
progressionHeader:SetPoint("TOPLEFT", progressionFrame, "TOPLEFT", 34, -72)
progressionHeader:SetText("Stat")

local progressionStoredHeader = progressionFrame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
progressionStoredHeader:SetPoint("TOP", progressionFrame, "TOP", 70, -72)
progressionStoredHeader:SetText("Stored")

local progressionAppliedHeader = progressionFrame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
progressionAppliedHeader:SetPoint("TOPRIGHT", progressionFrame, "TOPRIGHT", -38, -72)
progressionAppliedHeader:SetText("Applied")

local progressionScroll = CreateFrame(
    "ScrollFrame",
    "CrucibleProgressionScrollFrame",
    progressionFrame,
    "UIPanelScrollFrameTemplate"
)
progressionScroll:SetPoint("TOPLEFT", progressionFrame, "TOPLEFT", 28, -94)
progressionScroll:SetPoint("BOTTOMRIGHT", progressionFrame, "BOTTOMRIGHT", -48, 56)

local progressionContent = CreateFrame("Frame", nil, progressionScroll)
progressionContent:SetWidth(334)
progressionContent:SetHeight(1)
progressionScroll:SetScrollChild(progressionContent)

local progressionEmptyText = progressionContent:CreateFontString(nil, "OVERLAY", "GameFontDisable")
progressionEmptyText:SetPoint("TOP", progressionContent, "TOP", 0, -18)
progressionEmptyText:SetWidth(320)
progressionEmptyText:SetText("No absorbed stats recorded.")
progressionEmptyText:Hide()

local progressionStatus = progressionFrame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
progressionStatus:SetPoint("BOTTOMLEFT", progressionFrame, "BOTTOMLEFT", 30, 28)
progressionStatus:SetWidth(250)
progressionStatus:SetJustifyH("LEFT")
progressionStatus:SetText("")

local progressionRefreshButton = CreateFrame("Button", nil, progressionFrame, "UIPanelButtonTemplate")
progressionRefreshButton:SetWidth(86)
progressionRefreshButton:SetHeight(24)
progressionRefreshButton:SetPoint("BOTTOMRIGHT", progressionFrame, "BOTTOMRIGHT", -28, 20)
progressionRefreshButton:SetText("Refresh")

local progressionRows = {}

local function ClearProgressionRows()
    for _, row in ipairs(progressionRows) do
        row:Hide()
        row:SetParent(nil)
    end

    progressionRows = {}
end

local function FormatStoredValue(value)
    local n = tonumber(value)
    if not n then
        return tostring(value)
    end

    local formatted = string.format("%.4f", n)
    formatted = string.gsub(formatted, "0+$", "")
    formatted = string.gsub(formatted, "%.$", "")

    if n > 0 then
        return "+" .. formatted
    end

    return formatted
end

local function FormatAppliedValue(value)
    local n = tonumber(value)
    if not n then
        return tostring(value)
    end

    if n > 0 then
        return "+" .. tostring(n)
    end

    return tostring(n)
end

local function RenderProgression()
    ClearProgressionRows()

    progressionEmptyText:Hide()

    if #statsRows == 0 then
        progressionEmptyText:Show()
        progressionContent:SetHeight(80)
        progressionStatus:SetText("No accumulated bonuses")
        return
    end

    local rowHeight = 24

    for index, stat in ipairs(statsRows) do
        local row = CreateFrame("Frame", nil, progressionContent)
        row:SetWidth(330)
        row:SetHeight(rowHeight)
        row:SetPoint("TOPLEFT", progressionContent, "TOPLEFT", 0, -((index - 1) * rowHeight))

        local name = row:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
        name:SetPoint("LEFT", row, "LEFT", 6, 0)
        name:SetWidth(170)
        name:SetJustifyH("LEFT")
        name:SetText(stat.name)

        local stored = row:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
        stored:SetPoint("LEFT", row, "LEFT", 190, 0)
        stored:SetWidth(65)
        stored:SetJustifyH("RIGHT")
        stored:SetText(FormatStoredValue(stat.stored))

        local applied = row:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
        applied:SetPoint("RIGHT", row, "RIGHT", -6, 0)
        applied:SetWidth(55)
        applied:SetJustifyH("RIGHT")
        applied:SetText(FormatAppliedValue(stat.applied))

        table.insert(progressionRows, row)
    end

    progressionContent:SetHeight(math.max(1, #statsRows * rowHeight))
    progressionStatus:SetText(tostring(#statsRows) .. " accumulated stat" .. (#statsRows == 1 and "" or "s"))
end

local function RequestStats()
    statsReceiving = false
    statsRows = {}

    progressionStatus:SetText("Requesting stats...")
    progressionRefreshButton:Disable()

    SendAddonMessage(
        ADDON_PREFIX,
        "STATS",
        "WHISPER",
        UnitName("player")
    )
end

progressionRefreshButton:SetScript("OnClick", RequestStats)

progressionFrame:SetScript("OnShow", function()
    RequestStats()
end)

progressionFrame:Hide()

progressionButton:SetScript("OnClick", function()
    progressionFrame:Show()
    progressionFrame:Raise()
end)

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("BAG_UPDATE")

listener:SetScript("OnEvent", function(self, event, ...)
    if event == "BAG_UPDATE" then
        if selected and selected.guid then
            if RefreshSelectedLocation() then
                ApplyReservationVisual()
            elseif not absorbPending then
                ResetSlot("Selected item no longer available")
            end
        end
        return
    end

    local prefix, message, channel, sender = ...

    if prefix ~= ADDON_PREFIX then
        return
    end

    if message == "OPEN" then
        frame:Show()
        frame:Raise()
        return
    end

    if message == "CLOSE" then
        frame:Hide()
        return
    end

    local parts = SplitTabs(message)

    if parts[1] == "MASTERY_BEGIN" and parts[2] and parts[3] then
        local entry = tonumber(parts[2])
        if not selectedEssenceEntry or entry ~= selectedEssenceEntry then
            return
        end

        masteryPreviewEntry = entry
        masteryPreviewResult = parts[3]
        masteryStats = {}

        if parts[4] then masteryCurrent = tonumber(parts[4]) end
        if parts[5] then masteryNext = tonumber(parts[5]) end
        if parts[6] then masteryMoney = tonumber(parts[6]) or 0 end
        if parts[7] then masteryReagentEntry = tonumber(parts[7]) or 0 end
        if parts[8] then masteryReagentCount = tonumber(parts[8]) or 0 end
        return
    end

    if parts[1] == "MASTERY_STAT" and parts[2] and parts[3] and parts[4] then
        local entry = tonumber(parts[2])
        if entry ~= masteryPreviewEntry then
            return
        end

        table.insert(masteryStats, {
            name = parts[3],
            value = parts[4],
        })
        return
    end

    if parts[1] == "MASTERY_END" and parts[2] then
        local entry = tonumber(parts[2])
        if entry ~= masteryPreviewEntry then
            return
        end

        masteryUpgradePending = false
        masteryUpgradeButton:SetText("Upgrade")
        RenderMasteryPreview()
        return
    end

    if parts[1] == "MASTERY_RESULT" and parts[2] and parts[3] then
        local entry = tonumber(parts[2])
        if not selectedEssenceEntry or entry ~= selectedEssenceEntry then
            return
        end

        masteryUpgradePending = false
        masteryUpgradeButton:SetText("Upgrade")

        if parts[3] == "SUCCESS" then
            masteryStatus:SetText("Upgraded")
            RequestEssences()
        elseif parts[3] == "NOT_ENOUGH_MONEY" then
            masteryStatus:SetText("Not enough money")
            RenderMasteryPreview()
        elseif parts[3] == "NOT_ENOUGH_REAGENT" then
            masteryStatus:SetText("Missing reagent")
            RenderMasteryPreview()
        elseif parts[3] == "UNSUPPORTED_BRACKET" then
            masteryStatus:SetText("Unsupported bracket")
            RenderMasteryPreview()
        else
            masteryStatus:SetText("Upgrade failed: " .. tostring(parts[3]))
            RenderMasteryPreview()
        end
        return
    end

    if parts[1] == "ESSENCES_BEGIN" then
        essencesReceiving = true
        essenceRows = {}
        return
    end

    if parts[1] == "ESSENCE_ROW" and parts[2] and parts[3] then
        if not essencesReceiving then
            return
        end

        local entry = tonumber(parts[2])
        local mastery = tonumber(parts[3])

        if entry and mastery then
            table.insert(essenceRows, {
                entry = entry,
                mastery = mastery,
            })
        end

        return
    end

    if parts[1] == "ESSENCES_END" then
        if not essencesReceiving then
            return
        end

        essencesReceiving = false
        RenderEssences()
        return
    end

    if parts[1] == "STATS_BEGIN" then
        statsReceiving = true
        statsRows = {}
        return
    end

    if parts[1] == "STATS_ROW" and parts[2] and parts[3] and parts[4] and parts[5] then
        if not statsReceiving then
            return
        end

        table.insert(statsRows, {
            id = parts[2],
            name = parts[3],
            stored = parts[4],
            applied = parts[5],
        })
        return
    end

    if parts[1] == "STATS_END" then
        if not statsReceiving then
            return
        end

        statsReceiving = false
        progressionRefreshButton:Enable()
        RenderProgression()
        return
    end

    if parts[1] == "RESULT" and parts[2] and parts[3] then
        HandleResult(parts[2], parts[3])
        return
    end

    if parts[1] == "PREVIEW_BEGIN" and parts[2] and parts[3] then
        if not selected or parts[3] ~= selected.guid then
            return
        end

        previewGuid = parts[3]
        previewResult = parts[2]
        previewStats = {}
        return
    end

    if parts[1] == "PREVIEW_STAT" and parts[2] and parts[3] and parts[4] then
        if not selected or parts[2] ~= selected.guid or parts[2] ~= previewGuid then
            return
        end

        table.insert(previewStats, {
            name = parts[3],
            value = parts[4],
        })
        return
    end

    if parts[1] == "PREVIEW_END" and parts[2] then
        if not selected or parts[2] ~= selected.guid or parts[2] ~= previewGuid then
            return
        end

        RenderPreview()
        return
    end
end)

if ContainerFrame_Update then
    hooksecurefunc("ContainerFrame_Update", function()
        ApplyReservationVisual()
    end)
end

if ContainerFrame_UpdateLockedItem then
    hooksecurefunc("ContainerFrame_UpdateLockedItem", function()
        ApplyReservationVisual()
    end)
end

SLASH_CRUCIBLEUI1 = "/crucibleui"
SlashCmdList["CRUCIBLEUI"] = function()
    if frame:IsShown() then
        frame:Hide()
    else
        frame:Show()
        frame:Raise()
    end
end

SLASH_CRUCIBLESTATS1 = "/cruciblestats"
SlashCmdList["CRUCIBLESTATS"] = function()
    if progressionFrame:IsShown() then
        progressionFrame:Hide()
    else
        progressionFrame:Show()
        progressionFrame:Raise()
    end
end
