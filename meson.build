project('tll-bson', 'cpp'
	, version: '0.0.0'
	, license: 'MIT'
	, default_options: ['cpp_std=c++17', 'werror=true', 'optimization=2']
)

include = [include_directories('src')]

fmt = dependency('fmt')
bson = dependency('libbson-1.0')
tll = dependency('tll')

module = shared_library('tll-bson'
	, ['src/channel.cc']
	, include_directories : include
	, dependencies : [fmt, bson, tll]
	, install : true
)

install_subdir('src/tll', install_dir : get_option('includedir'))

benchmark('bench', executable('bench'
		, ['src/bench.cc']
		, include_directories : include
		, link_with: module
		, dependencies : [fmt, bson, tll]
		)
	, workdir : meson.current_source_dir()
)

test('pytest', import('python').find_installation('python3')
	, args: ['-m', 'pytest', '-v', 'tests']
	, env: 'BUILD_DIR=@0@'.format(meson.current_build_dir())
	, workdir: meson.current_source_dir()
)
