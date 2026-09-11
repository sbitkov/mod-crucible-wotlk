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
frame:SetWidth(420)
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
instruction:SetPoint("TOP", frame, "TOP", 0, -52)
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
previewTitle:SetPoint("TOPLEFT", frame, "TOPLEFT", 42, -192)
previewTitle:SetText("If absorbed:")

local previewText = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
previewText:SetPoint("TOPLEFT", previewTitle, "BOTTOMLEFT", 0, -8)
previewText:SetWidth(336)
previewText:SetJustifyH("LEFT")
previewText:SetJustifyV("TOP")
previewText:SetText("")

local statusText = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
statusText:SetPoint("BOTTOM", frame, "BOTTOM", 0, 56)
statusText:SetWidth(350)
statusText:SetText("")

local progressionButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
progressionButton:SetWidth(120)
progressionButton:SetHeight(24)
progressionButton:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 62, 22)
progressionButton:SetText("Progression")

local absorbButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
absorbButton:SetWidth(110)
absorbButton:SetHeight(24)
absorbButton:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -62, 22)
absorbButton:SetText("Absorb")
absorbButton:Disable()

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
    WEAPON_UNSUPPORTED = "Weapons are not supported",
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
        table.insert(lines, FormatAbsorbedValue(stat.value) .. " " .. stat.name)
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
