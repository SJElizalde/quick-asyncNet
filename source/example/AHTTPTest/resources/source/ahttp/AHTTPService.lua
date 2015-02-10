AHTTPService = inheritsFrom(baseClass)

------------- SERVICE CONSTANTS ---------------
MAX_CONCURRENT_REQUESTS = 5

---------- LOADER STATUS CONSTANTS ------------
LOADER_STATUS_WAITING="waiting"
LOADER_STATUS_LOADING="in_progress"
LOADER_STATUS_OPEN="begin"
LOADER_STATUS_CONNECTING="connecting"
LOADER_STATUS_ERROR="error"
LOADER_STATUS_COMPLETE="complete"

-- CLASS: AHTTPService ----------------

function AHTTPService:new()
  assert(ahttpService == nil)
  local obj = AHTTPService:create()
  AHTTPService:init(obj)
  return obj
end

function AHTTPService:init(obj)
  obj.downloads = {}
  obj.errors = {}
  obj.active_slots = 0
  system:addEventListener("http_event", obj)
end

function AHTTPService:isLoading(url)
  for idx, ldr in ipairs(self.downloads) do
    if ldr.url == url then return true end
  end
  return false
end

function AHTTPService:moveQueue()
  if self.active_slots >= MAX_CONCURRENT_REQUESTS then return end
  
  for idx, ldr in ipairs(self.downloads) do
    if ldr.status == LOADER_STATUS_WAITING and self.active_slots < MAX_CONCURRENT_REQUESTS then
      -- Open request for this file
      ahttp.downloadURL(ldr.url, ldr.filename)
      -- This is an interim status until C++ responds with "begin", it does not reflect an actual AHTTP status.
      ldr.status = LOADER_STATUS_CONNECTING
      self.active_slots = self.active_slots + 1
    end
  end
end

function AHTTPService:removeLoader(url)
  for idx, ldr in ipairs(self.downloads) do
    
    if ldr.url == url then
      self.active_slots = self.active_slots - 1
      table.remove(self.downloads, idx)
      if ldr.status == LOADER_STATUS_ERROR then
        table.insert(self.errors, ldr)
      end
      self:notify(ldr)
    end
  end
  self:moveQueue()
end

function AHTTPService:getLoader(url)
  for idx, ldr in ipairs(self.downloads) do
    if ldr.url == url then
      return ldr
    end
  end
  return nil
end

function AHTTPService:addLoader(url, filename)
  local data = {url=url, filename=filename, status=LOADER_STATUS_WAITING, listeners={}}
  table.insert(self.downloads, data)
  return data
end

-- Adds a file to the download queue
function AHTTPService:download(url, filename, listener)
  
  local download_data = nil
  if self:isLoading(url) then
    download_data = self:getLoader(url)
  else
    download_data = self:addLoader(url, filename)
  end
  table.insert(download_data.listeners, listener)
  self:moveQueue()
end

function AHTTPService:http_event(event)
  
  local loader_data = self:getLoader(event.url)
  
  -- This check avoids processing events related to downloads not handled by the service itself
  if not loader_data then
    return
  end
  
  loader_data.status = event.status
  if event.status == LOADER_STATUS_COMPLETE or event.status == LOADER_STATUS_ERROR then
    self:removeLoader(event.url)
  end
end

function AHTTPService:notify(ldr)
  -- Notify listeners of loader removal (sending final status)
  for i, listener in ipairs(ldr.listeners) do
    if listener.onLoadComplete then
      -- Object with named listener method
      listener:onLoadComplete(ldr.url, ldr.filename, ldr.status)
    else
      -- Anonymous listener function also supported
      listener(ldr.url, ldr.filename, ldr.status)
    end
  end
end

-- Returns the size of the current download queue
function AHTTPService:getQueueSize()
  return #self.downloads
end

ahttpService = AHTTPService.new();