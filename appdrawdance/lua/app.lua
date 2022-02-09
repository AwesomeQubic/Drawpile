-- Copyright (c) 2022 askmeaboutloom
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.
local EventBus <const> = require("event.bus")
local EventTypes <const> = require("event.types")
local LoginHandler <const> = require("session.login_handler")
local StateHandler <const> = require("session.state_handler")
local View <const> = require("view")

local App <const> = require("class")("App")

function App:init()
    self._event_bus = EventBus:new()
    self._view = View:new()
    self._login_handler = LoginHandler:new(self)
    self._state_handler = StateHandler:new(self)
    self._main_window = require("gui.main_window"):new(self)
    self._reload_gui = false
    self:subscribe_method(EventTypes.REQUEST_APP_QUIT, self)
    self:subscribe_method(EventTypes.REQUEST_GUI_RELOAD, self)
end

function App:publish(type, data)
    return self._event_bus:publish(type, data)
end

function App:subscribe(type, handler)
    return self._event_bus:subscribe(type, handler)
end

function App:subscribe_method(type, receiver, method)
    if not method then
        method = "on_" .. string.lower(type)
    end
    return self:subscribe(type, function (event)
        receiver[method](receiver, event)
    end)
end

function App:unsubscribe(type, handler)
    return self._event_bus:unsubscribe(type, handler)
end

function App:handle_events()
    self._event_bus:handle_events()
    local state = self:get_current_state()
    if state then
        self._view:handle_inputs(state.view_state)
    end
end

function App:_reload_main_window()
    unrequire(function (package)
        return string.find(package, "^gui%.")
            or string.find(package, "^translations%.")
            or package == "translations"
    end)
    local new_main_window = require("gui.main_window"):new(self)
    local old_main_window = self._main_window
    self._main_window = new_main_window
    old_main_window:dispose()
end

function App:prepare_gui()
    if self._reload_gui then
        self._reload_gui = false
        local ok, err = pcall(self._reload_main_window, self)
        if not ok then
            warn("Error in GUI reload: ", tostring(err))
        end
    end
    self._main_window:prepare()
    self._view:update_frame_inputs()
end

function App:dispose()
    self._main_window:dispose()
end

function App:on_request_app_quit()
    DP.App.quit()
end

function App:on_request_gui_reload()
    self._reload_gui = true
end

function App:get_states()
    return self._state_handler:get_states()
end

function App:get_current_state()
    return self._state_handler:get_current_state()
end

return App
