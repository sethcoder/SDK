#include "Script.h"

/**
	Because callbacks are handled in Lua, they still have to look like a Lua
	callback.  The Script wrapper class can't "rename" them.
**/
static int Script_PrintNumber(lua_State* state)
{
	// But we can enable Script support using a special constructor.
	Script script(state);

	// Retrieve the number passed on the stack.
	Script::Object numberObj = script.GetObject(1);

	// Verify it is a number and print it.
	if (numberObj.IsNumber())
		printf("%f\n", numberObj.GetNumber());

	// No return values.
	return 0;
}


/**
	Because callbacks are handled in Lua, they still have to look like a Lua
	callback.  The Script wrapper class can't "rename" them.
**/
static int Script_Add(lua_State* state)
{
	// But we can enable Script support using a special constructor.
	Script script(state);
	using Script::Object;		// So we don't have to type Script::Object.

	// Retrieve the numbers passed on the stack.
	Object number1Obj = script.GetObject(1);
	Object number2Obj = script.GetObject(2);

	// Verify it is a number and print it.
	if (number1Obj.IsNumber()  &&  number2Obj.IsNumber())
	{
		script.PushNumber(number1Obj.GetNumber() + number2Obj.GetNumber());
	}
	else
	{
		script.PushNumber(0.0f);
	}

	// 1 return value.
	return 1;
}


/**
	Demonstrate the use of the simple .ini script-like functions for writing
	values to a script.
**/
void DoScriptIniWriteTest()
{
	Script script;

	script.ConfigSetReal("Environment", "WindSpeed", 55.0);
	script.ConfigSetString("Environment", "TypeStr", "Overcast");
	script.ConfigSetInteger("AnotherGroup", "IntegerValue", 1);

	script.SaveText("ScriptIniTest.dmp");
}


/**
	Demonstrate the use of the simple .ini script-like functions for reading
	values from a script.
**/
void DoScriptIniReadTest()
{
	Script script;

	script.DoFile("ScriptIniTest.dmp");

	float windSpeed = script.ConfigGetReal("Environment", "WindSpeed", 0.0);
	const char* typeStr = script.ConfigGetString("Environment", "TypeStr", "Clear");
	int integerValue = script.ConfigGetInteger("AnotherGroup", "IntegerValue");
}


/**
	Demonstrate registering callback functions for the Lua script.
**/
void DoScriptCallbackTest()
{
	Script script;

	script.Register("PrintNumber", Script_PrintNumber);
	script.Register("Add", Script_Add);

	script.DoFile("ScriptCallbackTest.scr");
}


/**
	Demonstrate reading and saving a script.
**/
void DoScriptSaveTest()
{
	Script script;

	script.DoFile("ScriptSaveTest.scr");
	script.SaveText("ScriptSaveTest.dmp");
}


/**
	Demonstrates walking an array table.
**/
void DoScriptArrayTest()
{
	Script script;
	script.DoFile("ScriptArrayTest.scr");
	Script::Object testTableObj = script.GetGlobals().GetByName("TestArray");
	// or							script.GetGlobal("TestArray");
	for (int i = 1; ; ++i)
	{
		Script::AutoBlock block(script);	// Automatic stack fixups.
		Script::Object entryObj = testTableObj.GetByIndex(i);
		if (entryObj.IsNil())
			break;
		if (entryObj.IsNumber())		// IsNumber() must ALWAYS come first.
			printf("%f\n", entryObj.GetNumber());
		else if (entryObj.IsString())	// IsString() returns true for a number or string.
			printf("%s\n", entryObj.GetString());
	}
}


int main(int argc, char* argv[])
{
	DoScriptIniWriteTest();
	DoScriptIniReadTest();
	DoScriptCallbackTest();
	DoScriptSaveTest();
	DoScriptArrayTest();

	return 0;
}
