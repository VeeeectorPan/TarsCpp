project(GameServerClient)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../GameServer/src)
build_tars_server("GamServerClient" "")
