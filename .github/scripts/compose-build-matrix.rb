#!/usr/bin/env ruby
# frozen_string_literal: true

require 'json'
require 'yaml'

build_yaml = ARGV.fetch(0, 'build.yaml')
pattern = ARGV.fetch(1, 'all').to_s
pattern = '.*' if pattern.empty? || pattern == 'all'
regex = Regexp.new(pattern, Regexp::IGNORECASE)

data = YAML.load_file(build_yaml) || {}
attrs = ['board', 'shield', 'snippet', 'artifact-name', 'cmake-args']
targets = []

# Support the standard ZMK build.yaml matrix shape:
# board: [...]
# shield: [...]
# include: [...]
base_values = attrs.map do |attr|
  value = data[attr]
  value.nil? ? [nil] : Array(value)
end

unless base_values[0] == [nil]
  base_values[0].product(*base_values[1..]).each do |values|
    targets << attrs.zip(values).to_h.compact
  end
end

Array(data['include']).each do |entry|
  targets << entry.compact
end

targets.select! do |target|
  haystack = [
    target['artifact-name'],
    target['board'],
    target['shield'],
    target['snippet']
  ].compact.join(' ')

  haystack.match?(regex)
end

puts JSON.generate({ 'include' => targets })
