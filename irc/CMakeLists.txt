cmake_minimum_required(VERSION 2.8)

project(irc C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lpthread")

set(IRC_D ./ircd)
set(IRC_C ./ircc)

include_directories(${IRC_D})
include_directories(${IRC_C})

set(
        SERVER_SOURCE_FILES
        ${IRC_D}/linked_list.c ${IRC_D}/linked_list.h
        ${IRC_D}/client_handler.c ${IRC_D}/client_handler.h
        ${IRC_D}/channel.c ${IRC_D}/channel.h
        ${IRC_D}/logger.c ${IRC_D}/logger.h
        ${IRC_D}/config.c ${IRC_D}/config.h
        ${IRC_D}/commands.c ${IRC_D}/commands.h)

set(
        CLIENT_SOURCE_FILES
)

add_executable(server ${IRC_D}/server.c ${SERVER_SOURCE_FILES} )
add_executable(client ${IRC_C}/client.c ${CLIENT_SOURCE_FILES})