project('tll-bson', 'cpp'
	, version: '0.0.0'
	, license: 'MIT'
	, default_options: ['cpp_std=c++17', 'werror=true', 'optimization=2']
)

include = [include_directories('src')]

fmt = dependency('fmt')
bson = dependency('libbson-1.0')
tll = dependency('tll')

shared_library('tll-bson'
	, ['src/channel.cc']
	, include_directories : include
	, dependencies : [fmt, bson, tll]
	, install : true
)

#test('pytest', import('python').find_installation('python3')
#	, args: ['-m', 'pytest', '-v', 'tests']
#	, env: 'BUILD_DIR=@0@'.format(meson.current_build_dir())
#	, workdir: meson.current_source_dir()
#)
