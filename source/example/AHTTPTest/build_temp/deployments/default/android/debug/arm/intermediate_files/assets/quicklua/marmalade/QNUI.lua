--[[/*
 * (C) 2012-2013 Marmalade.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */--]]

--------------------------------------------------------------------------------
-- NUI singleton
--------------------------------------------------------------------------------
nui = quick.QNUI:new()

getmetatable(nui).__serialize = function(o)
	local obj = serializeTLMT(getmetatable(o), o)
	return obj
end

nui._webViewList = {}

--------------------------------------------------------------------------------
-- Private API
--------------------------------------------------------------------------------
function nui:_purge()
    -- Destroy all web views
    for i,v in pairs(self._webViewList) do
        v:destroy()
    end
    self._webViewList = {}
end

function nui:_syncWebViews()
	if (self._webViewList == nil) then
		return
	end

    -- Sync
    for i,v in ipairs(self._webViewList) do
        v:sync()
    end
end

