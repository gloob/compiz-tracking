find_package (Compiz REQUIRED)

include (CompizPlugin)

compiz_plugin (mag PLUGINDEPS composite opengl mousepoll)
