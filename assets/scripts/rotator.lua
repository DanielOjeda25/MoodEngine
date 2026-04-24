-- Script demo del Hito 8: rota la entidad sobre el eje Y.
-- Expuesto por LuaBindings: self.transform.rotationEuler.{x,y,z}
-- onUpdate es llamado cada frame por ScriptSystem con (self, dt).

local DEG_PER_SEC = 45.0

function onUpdate(self, dt)
    self.transform.rotationEuler.y = self.transform.rotationEuler.y + DEG_PER_SEC * dt
end
