
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