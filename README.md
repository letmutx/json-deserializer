# json-deserializer
JSON deserializer

* Follows the spec at https://json.org (No guarantees! :-))
* Recursively parses the input string.
* The deserializer itself doesn't assume any particular json structure, but the query system does. For sample structure, look at tests.
* Objects are hashtable with separate chaining
* Arrays are C arrays with type info.
* Numbers are floating point only.
* Duplicate keys at same level are **not** handled properly.
