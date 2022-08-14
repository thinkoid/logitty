project('tui', 'cpp', version : '0.0.1', license : 'WTFPL',
        default_options : ['cpp_std=c++20', 'build.cpp_std=c++20', 'werror=true'])

boost_dep = dependency('boost', modules : [ 'filesystem', 'iostreams', 'program_options', 'system' ])
fmt_dep = dependency('fmt')

test_dep = dependency('boost', modules : ['unit_test_framework'])

top_INCLUDES = include_directories('.')

tui_SOURCES = [ 'tui.cc' ]

tui = executable(
        'tui', tui_SOURCES, include_directories : [ top_INCLUDES ],
        dependencies : [ boost_dep, fmt_dep ])

