# MyScript
Very basic script language, based on LLVM, with c/c++ api

This is just a LLVM front end for my language. It parses script files then generate llvm IR and compiles it.
There is a visual studio extension for basic support [here](https://github.com/GuillaumeTrebuchet/MyScript-Language-Service)

# Features
The language is pretty simple, the goal was to make it fast, that's why there is no JIT compilation.
- function calls
- strong typing
- basic types: float, int, bool, string
- symbol listing and function call from the API
- imports (for API documentation in IDE)

# Syntax
The syntax looks like this:
``` lua
import "string.xml";
import "../test.xml";

string author_name = "myself";

function GetAuthorName() : string
	string s = strcat("author_name = ", author_name);
	s = strcat(s, "\n");
	print(s);
	return author_name;
end

function startswith(string s1, string s2) : bool
	return strcmp(substr(s1, 0, strlen(s2)), s2) == 0;
end

function isnullorempty(string s) : bool
	if(strcmp(s, null) == 0 or strcmp(s, "") == 0) then
		return true;
	else
		return false;
	end
end

function main()
	print("test\n");
end
```
