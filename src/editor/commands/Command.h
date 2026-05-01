#pragma once

// Comando del editor (Hito 27 Bloque 1): unidad atomica de mutacion del
// estado del editor que se puede deshacer.
//
// Diseño:
// - `ICommand` es polimorfico via virtual: `execute()` aplica el cambio,
//   `undo()` lo revierte. Cada comando guarda en sus miembros lo necesario
//   para reconstruir ambos lados (ej. transform anterior + nuevo).
// - `name()` devuelve un string corto para el menu "Editar > Deshacer X"
//   y los logs.
//
// Convencion: un comando bien escrito es REVERSIBLE — execute() dos veces
// no es idempotente, pero execute()->undo() debe dejar el estado igual al
// previo. Si tiene side-effects no-reversibles (ej. crear archivos en
// disco), no es candidato a Command.

#include <entt/entity/entity.hpp>

#include <string>

namespace Mood {

class ICommand {
public:
    virtual ~ICommand() = default;

    /// @brief Aplica el cambio. Llamado al hacer la accion original Y al
    ///        rehacerla con Ctrl+Y.
    virtual void execute() = 0;

    /// @brief Revierte el cambio aplicado por `execute()`. Llamado en Ctrl+Z.
    ///        Asume que el estado en memoria es el resultado de execute().
    virtual void undo() = 0;

    /// @brief Texto corto para el menu y logs ("Mover entidad",
    ///        "Eliminar Cubo", etc.).
    virtual std::string name() const = 0;

    /// @brief Hito 32: cuando un comando del stack recrea una entidad
    ///        destruida (ej. DeleteEntityCommand::undo), el handle EnTT
    ///        viejo queda invalidado por versionado. Este hook permite
    ///        a comandos previos del stack patchear su `Entity` interna
    ///        para apuntar al nuevo handle. Default no-op para comandos
    ///        que no referencian entidades especificas.
    virtual void onEntityRemap(entt::entity /*oldH*/, entt::entity /*newH*/) {}
};

} // namespace Mood
