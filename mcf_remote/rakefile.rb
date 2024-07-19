if system 'sccache -s 1> /dev/null 2> /dev/null' then
  SCCACHE_ADAPT = '--adapt sccache'.freeze
else
  SCCACHE_ADAPT = ''.freeze
end

task :build do
  sh "bake PerfReceiver --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -a black"
  sh "bake PerfSender --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -a black"
  sh "bake Simple --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -a black"
  sh "bake Generator --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -a black"
  sh "bake Responder --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -a black"
end

task :clean do
  sh "bake PerfReceiver --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -c"
  sh "bake PerfSender --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -c"
  sh "bake Simple --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -c"
  sh "bake Generator --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -c"
  sh "bake Responder --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -c"
  sh "bake -m test/mcf_remote_value_types Lib --adapt admin/adapt/gcc #{SCCACHE_ADAPT} -c"
end
