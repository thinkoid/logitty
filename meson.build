project('tui', 'c', version : '0.0.1', license : 'WTFPL')

ncurses_dep = dependency('ncurses')
form_dep = dependency('form')

top_INCLUDES = include_directories('.')

tui_SOURCES = [ 'tui.c' ]

tui = executable(
        'tui', tui_SOURCES, include_directories : [ top_INCLUDES ],
        dependencies : [ ncurses_dep, form_dep ])

