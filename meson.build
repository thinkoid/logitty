project('tui', 'cpp', version : '0.0.1', license : 'WTFPL',
        default_options : ['cpp_std=c++20', 'build.cpp_std=c++20' ])

ncurses_dep = dependency('ncurses')
form_dep = dependency('form')

top_INCLUDES = include_directories('.')

tui_SOURCES = [ 'tui.cc' ]

tui = executable(
        'tui', tui_SOURCES, include_directories : [ top_INCLUDES ],
        dependencies : [ ncurses_dep, form_dep ])

