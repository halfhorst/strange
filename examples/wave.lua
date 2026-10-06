-- A scrolling sine wave, as a Lua screensaver.
--
-- Copy this file into ~/.strange/ and run `strange wave`.

local glyphs = { ".", "o", "O", "o" }

return {
  name = "wave",

  init = function(buffer)
    return { started = nil }
  end,

  update = function(state, buffer, frame)
    state.started = state.started or frame.time
    local elapsed = frame.time - state.started
    local middle = (buffer.h - 1) / 2

    for x = 0, buffer.w - 1 do
      local phase = x / 6 + elapsed * 3
      local y = math.floor(middle + math.sin(phase) * middle * 0.8 + 0.5)
      buffer:write(glyphs[(x + frame.frame_count // 8) % #glyphs + 1], x, y)
    end

    buffer:write_string(string.format(" %dx%d ", buffer.w, buffer.h), 0, buffer.h - 1)
  end,
}
