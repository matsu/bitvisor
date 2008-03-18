#!/usr/bin/ruby

keys = Array.new
types = Hash.new
defaults = Hash.new
ids = Hash.new

while line = gets do
      next if line =~ /^#/
      (key, type, default) = line.split
      keys.push(key)
      types[key] = "CONFIG_TYPE_" + type.upcase
      defaults[key] = default
end

keys.each do |key|
      id = "CONFIG_" + key.upcase.gsub(/\./, "_")
      ids[key] = id
end

idlist = ""; values = ""; code = "";
keys.each do |key|
    idlist << "#{ids[key]},\n"
    values << "#{types[key]}, #{defaults[key]},\n"
    code <<   "\tstrcmp(key, \"#{key}\") == 0 ?\\\n\t\t&value_list[#{ids[key]}] :\\\n"
end


print <<"EOF"
#include "config.h"
#define NULL (void *)(0)

enum {
#{idlist}
};

static const struct config_value value_list[] = {
#{values}
};

#define config_get(key) \\
#{code}	NULL

int main()
{
	printf("%x\\n", config_get("vmm.idman.password.algorithm"));
}

EOF
