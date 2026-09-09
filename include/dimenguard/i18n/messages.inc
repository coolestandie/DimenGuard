DG_MESSAGE(Help, "Commands", "Comandos")
DG_MESSAGE(RegionHasChildren, "Detach this region's children before deleting it.",
           "Desvincula las regiones hijas antes de eliminar esta región.")
DG_MESSAGE(InvalidHierarchy, "Invalid hierarchy. Check the parent, priorities and hierarchy depth.",
           "Jerarquía inválida. Revisa el padre, las prioridades y la profundidad de la jerarquía.")
DG_MESSAGE(InvalidRegionType, "This operation is not available for the dimension's global region.",
           "Esta operación no está disponible para la región global de la dimensión.")
DG_MESSAGE(HelpSelection, "Selection", "Selección")
DG_MESSAGE(HelpRegions, "Regions", "Regiones")
DG_MESSAGE(HelpProtection, "Protection", "Protección")
DG_MESSAGE(HelpGeneral, "General", "General")
DG_MESSAGE(HelpSelectFirst, "Select the first corner.", "Selecciona la primera esquina.")
DG_MESSAGE(HelpSelectSecond, "Select the second corner.", "Selecciona la segunda esquina.")
DG_MESSAGE(HelpCreate, "Protect your selection.", "Protege tu selección.")
DG_MESSAGE(HelpDelete, "Remove a region.", "Elimina una región.")
DG_MESSAGE(HelpRename, "Change its name.", "Cambia su nombre.")
DG_MESSAGE(HelpList, "Browse this dimension.", "Consulta esta dimensión.")
DG_MESSAGE(HelpInfo, "View bounds and rules.", "Consulta límites y reglas.")
DG_MESSAGE(HelpPriority, "Resolve overlapping regions.", "Resuelve regiones superpuestas.")
DG_MESSAGE(HelpFlag, "View or change flags.", "Consulta o cambia flags.")
DG_MESSAGE(HelpFlags, "Browse available flags by page.", "Consulta las flags por página.")
DG_MESSAGE(HelpTrust, "Trust an online player.", "Autoriza a un jugador conectado.")
DG_MESSAGE(HelpUntrust, "Remove an online member.", "Retira a un miembro conectado.")
DG_MESSAGE(HelpShow, "Open this command guide.", "Abre esta guía de comandos.")
DG_MESSAGE(HelpLanguage, "Choose your session language.", "Elige el idioma de tu sesión.")
DG_MESSAGE(HelpReload, "Reload saved regions.", "Recarga las regiones guardadas.")
DG_MESSAGE(InvalidPage, "Choose an existing positive page number.", "Elige un número de página positivo que exista.")
DG_MESSAGE(Usage, "Invalid arguments. Use /dg help for command syntax.",
           "Argumentos inválidos. Usa /dg help para ver la sintaxis.")
DG_MESSAGE(PlayerOnly, "Run this command in game to select a dimension.",
           "Ejecuta este comando en el juego para seleccionar una dimensión.")
DG_MESSAGE(NoPermission, "You do not have permission to use this command.", "No tienes permiso para usar este comando.")
DG_MESSAGE(NotReady, "Region storage is unavailable. Contact an administrator.",
           "El almacenamiento de regiones no está disponible. Contacta a un administrador.")
DG_MESSAGE(Failed, "The command could not finish. Check the server log and region state before retrying.",
           "El comando no pudo terminar. Revisa el registro y el estado de la región antes de reintentarlo.")
DG_MESSAGE(Selected, "Position {0}: {1}, {2}, {3} in {4}.", "Posición {0}: {1}, {2}, {3} en {4}.")
DG_MESSAGE(SelectionRequired, "Set /dg pos1 and /dg pos2 in the same level and dimension first.",
           "Primero usa /dg pos1 y /dg pos2 en el mismo mundo y dimensión.")
DG_MESSAGE(Created, "Region '{0}' created. Build and interaction are members-only by default.",
           "Región '{0}' creada. Por defecto, solo sus miembros pueden construir e interactuar.")
DG_MESSAGE(Deleted, "Region '{0}' deleted.", "Región '{0}' eliminada.")
DG_MESSAGE(Renamed, "Region '{0}' renamed to '{1}'.", "Región '{0}' renombrada a '{1}'.")
DG_MESSAGE(NotFound, "That region does not exist in this dimension.", "Esa región no existe en esta dimensión.")
DG_MESSAGE(Exists, "That region name already exists in this dimension.",
           "Ese nombre de región ya existe en esta dimensión.")
DG_MESSAGE(InvalidName, "Use 1-64 lowercase letters, digits, underscores or hyphens. '__global__' is reserved.",
           "Usa de 1 a 64 letras minúsculas, números, guiones o guiones bajos. '__global__' está reservado.")
DG_MESSAGE(InvalidNumber, "The priority must be a valid 32-bit integer.",
           "La prioridad debe ser un entero válido de 32 bits.")
DG_MESSAGE(List, "Regions in {0}: {1} (page {2}/{3}).", "Regiones en {0}: {1} (página {2}/{3}).")
DG_MESSAGE(ListItem, "{0}  [priority {1}]", "{0}  [prioridad {1}]")
DG_MESSAGE(
    Info, "{0} | {1} | priority {2}\nBounds: ({3}, {4}, {5}) to ({6}, {7}, {8})\nOwner UUID: {9} | members: {10}",
    "{0} | {1} | prioridad {2}\nLímites: ({3}, {4}, {5}) a ({6}, {7}, {8})\nUUID del propietario: {9} | miembros: {10}")
DG_MESSAGE(FlagInfo, "{0}: {1}", "{0}: {1}")
DG_MESSAGE(FlagsAvailable, "Available flags", "Flags disponibles")
DG_MESSAGE(FlagsPage, "{0} (page {1}/{2}, {3} flags)", "{0} (página {1}/{2}, {3} flags)")
DG_MESSAGE(FlagScopePlayer, "Player actions", "Acciones de jugadores")
DG_MESSAGE(FlagScopeEnvironment, "World and actor rules", "Reglas del mundo y entidades")
DG_MESSAGE(FlagScopeTransition, "Movement", "Movimiento")
DG_MESSAGE(FlagNavigation, "Browse: {0}", "Consulta: {0}")
DG_MESSAGE(FlagBuildDescription, "Player block breaking, placement and supported bucket actions.",
           "Rotura y colocación de bloques por jugadores y usos compatibles de cubetas.")
DG_MESSAGE(FlagInteractDescription, "Supported right-click block and actor interactions.",
           "Interacciones compatibles con clic derecho en bloques y con entidades.")
DG_MESSAGE(FlagContainerDescription, "Opening interactions for recognized block containers.",
           "Interacciones para abrir contenedores de bloques reconocidos.")
DG_MESSAGE(FlagPvpDescription, "Damage between players when the attacker can be identified.",
           "Daño entre jugadores cuando se puede identificar al atacante.")
DG_MESSAGE(FlagStates, "States: {0}.", "Estados: {0}.")
DG_MESSAGE(FlagExplosionsDescription, "Reported explosion block destruction and damage.",
           "Destrucción de bloques y daño por explosiones registrados.")
DG_MESSAGE(FlagFluidFlowDescription, "Reported fluid flow at its source and destination.",
           "Flujo registrado de fluidos en origen y destino.")
DG_MESSAGE(FlagBlockFormDescription, "Lava solidification only; other block formation is not covered.",
           "Solo solidificación de lava; no cubre otras formaciones.")
DG_MESSAGE(FlagLeafDecayDescription, "Reported natural leaf decay.", "Desaparición natural de hojas registrada.")
DG_MESSAGE(FlagActorGriefingDescription, "Reported actor-driven block changes.",
           "Cambios de bloques registrados causados por entidades.")
DG_MESSAGE(FlagMobSpawningDescription, "Added non-player mobs are removed when denied.",
           "Elimina mobs al añadirse si se deniega; no afecta a jugadores.")
DG_MESSAGE(FlagMobDamageDescription, "Damage to players attributed to a mob.", "Daño a jugadores atribuido a un mob.")
DG_MESSAGE(FlagEntryDescription, "Entering on reported moves/teleports; portal coverage is incomplete.",
           "Entrada por movimientos/teletransportes registrados; portales parciales.")
DG_MESSAGE(FlagExitDescription, "Leaving on reported moves/teleports; portal coverage is incomplete.",
           "Salida por movimientos/teletransportes registrados; portales parciales.")
DG_MESSAGE(FlagBlockBreakDescription, "Player block breaking; falls back to build.",
           "Rotura de bloques por jugadores; hereda build.")
DG_MESSAGE(FlagBlockPlaceDescription, "Player block placement; falls back to build.",
           "Colocación de bloques por jugadores; hereda build.")
DG_MESSAGE(FlagUseDescription, "Supported block use; falls back to interact.",
           "Uso compatible de bloques; hereda interact.")
DG_MESSAGE(FlagUseAnvilDescription, "Opening supported anvils; falls back to use.",
           "Apertura de yunques compatibles; hereda use.")
DG_MESSAGE(FlagSleepDescription, "Bed entry; inherits interact, which must also allow the click.",
           "Acostarse; hereda interact y también requiere permitir el clic.")
DG_MESSAGE(FlagItemDropDescription, "Active player item drops; excludes death drops.",
           "Objetos soltados por jugadores vivos; excluye muertes.")
DG_MESSAGE(FlagItemPickupDescription, "Reported player and mob pickup; checks both endpoints.",
           "Recogida registrada por jugadores y mobs; revisa ambos extremos.")
DG_MESSAGE(FlagSendChatDescription, "Player chat reported by this server.",
           "Chat de jugadores registrado por este servidor.")
DG_MESSAGE(FlagWaterFlowDescription, "Reported water flow; falls back to fluid-flow.",
           "Flujo de agua registrado; hereda fluid-flow.")
DG_MESSAGE(FlagLavaFlowDescription, "Reported lava flow; falls back to fluid-flow.",
           "Flujo de lava registrado; hereda fluid-flow.")
DG_MESSAGE(FlagFallDamageDescription, "Reported fall damage to mobs and players.",
           "Daño de caída registrado en mobs y jugadores.")
DG_MESSAGE(FlagFireworkDamageDescription, "Reported firework damage to mobs and players.",
           "Daño de fuegos artificiales registrado en mobs y jugadores.")
DG_MESSAGE(FlagDefaults, "Defaults: build/interact/container-access require membership.",
           "Por defecto: build/interact/container-access exigen membresía.")
DG_MESSAGE(FlagFallbacks, "Unset granular flags follow build, interact, use or fluid-flow.",
           "Las flags específicas sin valor heredan build, interact, use o fluid-flow.")
DG_MESSAGE(FlagSyntax, "Usage: {0}", "Uso: {0}")
DG_MESSAGE(FlagExample, "Example: /dg flag spawn pvp deny", "Ejemplo: /dg flag spawn pvp deny")
DG_MESSAGE(RegionFlags, "Region '{0}': stored flags", "Región '{0}': flags guardadas")
DG_MESSAGE(FlagInvincibleDescription, "allow blocks reported player damage; default deny.",
           "allow bloquea daño recibido por jugadores; por defecto deny.")
DG_MESSAGE(FlagImmunityDefault, "Other flags default to allow; invincible uses deny (no immunity).",
           "Las demás usan allow por defecto; invincible usa deny (sin inmunidad).")
DG_MESSAGE(FlagInheritance, "inherit follows overlapping regions and default rules.",
           "inherit consulta las regiones superpuestas y las reglas predeterminadas.")
DG_MESSAGE(FlagSet, "Region '{0}': {1} = {2}.", "Región '{0}': {1} = {2}.")
DG_MESSAGE(InvalidFlag, "Unknown flag or state. Use /dg flags to view the available options.",
           "Flag o estado desconocido. Usa /dg flags para ver las opciones disponibles.")
DG_MESSAGE(PrioritySet, "Region '{0}' priority set to {1}.", "Prioridad de la región '{0}' establecida en {1}.")
DG_MESSAGE(PlayerNotFound, "Choose one online player by name.", "Elige a un jugador conectado por su nombre.")
DG_MESSAGE(MemberSet, "Region '{0}': membership updated for {1}.", "Región '{0}': membresía actualizada para {1}.")
DG_MESSAGE(Reloaded, "Loaded {0} regions from storage.", "Se cargaron {0} regiones del almacenamiento.")
DG_MESSAGE(LanguageSet, "Language: {0}. This setting lasts for your current session.",
           "Idioma: {0}. Esta opción dura hasta que termine tu sesión.")
DG_MESSAGE(Denied, "This region does not allow that action.", "Esta región no permite esa acción.")
DG_MESSAGE(LimitReached, "The region limit has been reached (10,000).", "Se alcanzó el límite de regiones (10 000).")
