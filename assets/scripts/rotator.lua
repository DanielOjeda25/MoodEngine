-- Script demo del Hito 8: rota la entidad sobre el eje Y.
-- Hito 24: usa engine.exposed para que la velocidad sea editable
-- desde el Inspector + persistible en `.moodmap` per-entity.

local speed = engine.exposed("speed", 85.0)  -- grados por segundo

function onUpdate(self, dt)
    self.transform.rotationEuler.y =
        self.transform.rotationEuler.y + speed * dt
end
