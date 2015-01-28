require("mobdebug").start()
--TEST_FILE_URL = "http://mirror.internode.on.net/pub/test/1meg.test"
TEST_FILE_URL = "http://storage.googleapis.com/tulum/book/105/pt/pages/"

local current_dls = 0
local total_dls = 0
local progress = {}
local label = director:createLabel(0, 0, "READY")
label.color = color.red
label.isVisible = false

local progress_label = director:createLabel(0, 0, "")

local progress_string = ""

local button = director:createSprite(director.displayCenterX, director.displayHeight-100, "img/button.png")
button.xAnchor = 0.5
button.yAnchor = 0.5
button.xScale = 0.4
button.yScale = 0.4

function button:touch(event)
	if (event.phase == "began") then
    total_dls = 0
    start_dl()
    label.text = "PROCESSING " .. current_dls .. " FILES"
	end
end

function start_dl()
   while current_dls < 5 and total_dls < 150 do
      current_dls = current_dls + 1
      total_dls = total_dls + 1
      local filename = "file_" .. total_dls .. ".jpg"
      print("CALLING AHTTP to Download: " .. filename)
      local url = TEST_FILE_URL .. (total_dls % 8) + 1 .. "_720.jpg"
      --local url = TEST_FILE_URL .. "1_720.jpg"
      ahttp.downloadURL(url, filename)
    end
end

function onComplete(event)
  
  if event.status == "complete" then
    current_dls = current_dls - 1
    progress[event.filename] = nil
    print("DOWNLOAD COMPLETE: " .. event.filename .. ", [" .. current_dls .. "] Active")
    start_dl()
  elseif event.status == "in_progress" then
    local pstring = tostring(event.percent * 100)
    progress[event.filename] = string.sub(pstring, 0, 3) .. "%"
    
  elseif event.status == "error" then
    progress[event.filename] = "ERROR " .. event.ecode
    current_dls = current_dls - 1
    print("DOWNLOAD ERROR: " .. event.ecode .. "[" .. current_dls .. "] Active")
    start_dl()
  end
end

function onUpdate(event)
  
  -- Rotate button graphic to demonstrate non-blocking threaded downloads
  if current_dls > 0 then
    if button.rotation < 360 then
      button.rotation = button.rotation + 1
    else
      button.rotation = 0
    end
    progress_string = "Downloads in progress:\n"
    -- Show progress of downloads in a text field
    for name, pstring in pairs(progress) do
      --print(filename .. "->" .. pstring)
      local part = ">>>> ".. name .. ": " .. pstring .. "\n"
      progress_string = progress_string .. part
    end
     progress_label.text = progress_string
  elseif button.rotation ~= 0 then
    button.rotation = 0
    --progress_string = "No downloads in progress"
  end
 
end

system:addEventListener("http_event", onComplete)
system:addEventListener("update", onUpdate)
button:addEventListener("touch", button)

