# Ipremap

Ipremap is terrible NAT kludge.

You can use Ipremap to dynamically add DNAT rules to a given iptables
chain with optional rule removal after a given time-to-live.

A simple use-case is letting applications influence `fwmark` routing
at will. Your application may remap the IP it wants to connect into a
private range, e.g. 10.19.0.0/16 and connect to the remapped IP. You
can use `iptables` rule add the desired mark to the packets going to
the remapped IP. The DNAT rule added by Ipremap will then direct the
traffic to its original destination.

## Usage

It is generally not advised to use Ipremap.

For now, if you really want to create a routing mess, look at
`ipremapd.cc` for configuration possibilities and `ipremap.c` for an
example client. Someday a proper user interface may be added.

## License

Copyright (C) 2014 Kristof Marussy kris7topher@gmail.com

Ipremap is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Ipremap is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Ipremap.  If not, see http://www.gnu.org/licenses/.
