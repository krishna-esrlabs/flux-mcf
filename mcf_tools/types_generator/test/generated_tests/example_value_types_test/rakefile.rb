if system 'sccache -s 1> /dev/null 2> /dev/null' then
  SCCACHE_ADAPT = '--adapt sccache'.freeze
else
  SCCACHE_ADAPT = ''.freeze
end

task :build do
  sh "bake --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -a black"
end

task :clean do
  sh "bake --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -c"
end
