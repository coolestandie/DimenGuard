// Each entry defines one message in both supported locales.
DG_MESSAGE(Help, "Commands", "Comandos")
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
DG_MESSAGE(HelpFlag, "Set allow, deny or inherit.", "Establece allow, deny o inherit.")
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
DG_MESSAGE(InvalidName, "Use 1-64 lowercase letters, digits, underscores or hyphens for region names.",
           "Usa de 1 a 64 letras minúsculas, números, guiones o guiones bajos en los nombres.")
DG_MESSAGE(InvalidNumber, "The priority must be a valid 32-bit integer.",
           "La prioridad debe ser un entero válido de 32 bits.")
DG_MESSAGE(List, "Regions in {0}: {1} (page {2}/{3}).", "Regiones en {0}: {1} (página {2}/{3}).")
DG_MESSAGE(ListItem, "{0}  [priority {1}]", "{0}  [prioridad {1}]")
DG_MESSAGE(
    Info, "{0} | {1} | priority {2}\nBounds: ({3}, {4}, {5}) to ({6}, {7}, {8})\nOwner UUID: {9} | members: {10}",
    "{0} | {1} | prioridad {2}\nLímites: ({3}, {4}, {5}) a ({6}, {7}, {8})\nUUID del propietario: {9} | miembros: {10}")
DG_MESSAGE(FlagInfo, "{0}: {1}", "{0}: {1}")
DG_MESSAGE(FlagSet, "Region '{0}': {1} = {2}.", "Región '{0}': {1} = {2}.")
DG_MESSAGE(InvalidFlag, "Flags: build, interact, container-access, pvp. States: allow, deny, inherit.",
           "Flags: build, interact, container-access, pvp. Estados: allow, deny, inherit.")
DG_MESSAGE(PrioritySet, "Region '{0}' priority set to {1}.", "Prioridad de la región '{0}' establecida en {1}.")
DG_MESSAGE(PlayerNotFound, "Choose one online player by name.", "Elige a un jugador conectado por su nombre.")
DG_MESSAGE(MemberSet, "Region '{0}': membership updated for {1}.", "Región '{0}': membresía actualizada para {1}.")
DG_MESSAGE(Reloaded, "Loaded {0} regions from storage.", "Se cargaron {0} regiones del almacenamiento.")
DG_MESSAGE(LanguageSet, "Language: {0}. This setting lasts for your current session.",
           "Idioma: {0}. Esta opción dura hasta que termine tu sesión.")
DG_MESSAGE(Denied, "This region does not allow that action.", "Esta región no permite esa acción.")
DG_MESSAGE(LimitReached, "The region limit has been reached (10,000).", "Se alcanzó el límite de regiones (10 000).")
