#include "c4d.h"

#include "maxon/vm.h"
#include "maxon/cpython.h"
#if (API_VERSION < 23000)
#include "maxon/cpython27_raw.h"
#elif (API_VERSION >= 23000 && API_VERSION < 24000)
#include "maxon/cpython37_raw.h"
#else
#include "maxon/cpython3_raw.h"
#endif

#if (API_VERSION >= 22000)
	#include "maxon/errortypes.h"
#endif

#include "c4d_general.h"
#include "c4d_basedocument.h"
#include "lib_py.h"

#define PLUGIN_CALLPYTHON_PREFID 1057035
#define ID_CALLPYTHON_SCENEHOOK 1056807

// Docs
// https://developers.maxon.net/docs/Cinema4DCPPSDK/html/page_maxonapi_python.html#page_maxonapi_python_classes
static maxon::Result<void> ExecutePythonScript(const Filename& sourceFile , BaseDocument* doc)
{
	iferr_scope;

#if (API_VERSION < 23000)
	const maxon::VirtualMachineRef&     vm = MAXON_CPYTHON27VM();
#elif (API_VERSION >= 23000 && API_VERSION < 24000)
	const maxon::VirtualMachineRef&     vm = MAXON_CPYTHON37VM();
#else
	const maxon::VirtualMachineRef&     vm = MAXON_CPYTHON3VM();
#endif

	const maxon::VirtualMachineScopeRef scope = vm.CreateScope() iferr_return;

	maxon::String sourceCode;
	AutoAlloc<BaseFile> file;
	if (!file->Open(sourceFile))
	{
		const String errorMessage = "File not found"_s;
		return maxon::UnknownError(MAXON_SOURCE_LOCATION, errorMessage);
	}

	Int64 dataSize = file->GetLength();
	iferr(Char *data = NewMemClear(Char, dataSize + 1))
	{
		const String errorMessage = "Could not allocate data to hold file"_s;
		return maxon::UnknownError(MAXON_SOURCE_LOCATION, errorMessage);
	}

	if (data)
	{
		if (file->ReadBytes(data, dataSize) == dataSize)
		{
			iferr(sourceCode.SetCString(data, dataSize))
			{

			}
		}
		DeleteMem(data);
	}

	if (sourceCode.IsEmpty())
	{
		const String errorMessage = "File empty"_s;
		return maxon::UnknownError(MAXON_SOURCE_LOCATION, errorMessage);
	}

	// init script
	iferr(scope.Init("Python Script"_s, sourceCode, maxon::ERRORHANDLING::PRINT, nullptr))
	{
		const String errorMessage = "Error on Init()"_s;
		return maxon::UnknownError(MAXON_SOURCE_LOCATION, errorMessage);
	}

	// only set "doc" and "op" if a BaseDocument is set.
	// another option would be to set both "doc" and "op" to None
	if (doc != nullptr)
	{
		// set "doc" and "op" variables

		auto python = maxon::Cast<maxon::py::CPythonLibraryRef>(vm.GetLibraryRef());
		maxon::py::CPythonGil gil(python);

		// the new Python API from python.framework is compatible with the legacy Python API from lib_py.h.
		// here we temporarily create an old PythonLibrary instance because it can create a PyObject
		// from a BaseDocument and BaseObject
		PythonLibrary pyLib;

		auto* pyObjectDoc = pyLib.ReturnGeListNode(doc, false);
		if (pyObjectDoc == nullptr)
			return maxon::UnexpectedError(MAXON_SOURCE_LOCATION);

		auto* pyDoc = reinterpret_cast<maxon::py::NativePyObject*>(pyObjectDoc);
		scope.Add("doc"_s, maxon::Data(pyDoc)) iferr_return;
		python.CPy_Decref(pyDoc);


		BaseObject* op = doc->GetActiveObject();

		auto* pyObjectOp = op ? pyLib.ReturnGeListNode(op, false) : pyLib.ReturnPyNone();
		if (pyObjectOp == nullptr)
			return maxon::UnexpectedError(MAXON_SOURCE_LOCATION);

		auto* pyOp = reinterpret_cast<maxon::py::NativePyObject*>(pyObjectOp);
		scope.Add("op"_s, maxon::Data(pyOp)) iferr_return;
		python.CPy_Decref(pyOp);
	}

	// set __name__ = __main__
	scope.Add("__name__"_s, maxon::Data("__main__"_s)) iferr_return;

	// stop all current threads which are potentially modifying the current document,
	// since the active script might modify the currently active document
	StopAllThreads();

	// executes the script and returns when it got executed.
	// info: if the script causes an unexpected infinite loop, Execute() does not return
	// and there is no way to stop from the outside.
	iferr(scope.Execute())
	{
		const String errorMessage = "Error on Execute()"_s;
		return maxon::UnknownError(MAXON_SOURCE_LOCATION, errorMessage);
	}

	return maxon::OK;
}

class StartupSceneHook : public SceneHookData
{
	INSTANCEOF(StartupSceneHook, SceneHookData)

public:
	StartupSceneHook() {}
	virtual ~StartupSceneHook() { }

	static NodeData *Alloc(void) { return NewObjClear(StartupSceneHook); }

	virtual Bool Message(GeListNode *node, Int32 type, void *data)
	{
		switch (type)
		{
		case MSG_DOCUMENTINFO:
		{
			DocumentInfoData *pInfoData = (DocumentInfoData*)data;
			BaseDocument *doc = pInfoData->doc;
			switch (pInfoData->type)
			{
			case MSG_DOCUMENTINFO_TYPE_LOAD:
			{
				Filename fn = GeGetPluginPath();
				fn += Filename("aftersceneload.py");
				if (!GeFExist(Filename(fn)))
					return false;

				iferr(ExecutePythonScript(fn, doc))
				{

				}
			}
			break;
			};
		}
		break;
		default:
			break;
		}
		return SceneHookData::Message(node, type, data);
	}
};

Bool PluginStart()
{
	if (!RegisterSceneHookPlugin(ID_CALLPYTHON_SCENEHOOK, "CallPython"_s, PLUGINFLAG_HIDE, StartupSceneHook::Alloc, 0, 0)) return false;
	GeConsoleOut("CallPython Loaded"_s);
	return true;
}

void PluginEnd()
{
}

Bool PluginMessage(Int32 id, void* data)
{
	switch (id)
	{
		case C4DPL_INIT_SYS:
			return true;

		case C4DMSG_PRIORITY:
			return true;

		case C4DPL_BUILDMENU:
			break;

		case C4DPL_ENDACTIVITY:
			return true;

		case C4DPL_COMMANDLINEARGS:
		{
			String pythonToRun;

			const C4DPL_CommandLineArgs* const args = static_cast<C4DPL_CommandLineArgs*>(data);
			if (args == nullptr)
				return false;

			// loop through all command line arguments
			for (Int32 i = 0; i < args->argc; i++)
			{
				// check if argument is set
				if (!args->argv[i])
					continue;

				// check if this is the "custom" argument
				if (strcmp(args->argv[i], "-runpython") == 0)
				{
					// set to nullptr so it won't confuse other modules
					args->argv[i] = nullptr;

					// get the argument that is the next command line element
					i++;
					pythonToRun = String(args->argv[i]);

					// print result
					ApplicationOutput("Argument: " + pythonToRun);

					// set to nullptr so it won't confuse other modules
					args->argv[i] = nullptr;

					break;
				}
			}

			if (!GeFExist(Filename(pythonToRun)))
				return false;

			iferr(ExecutePythonScript(pythonToRun, GetActiveDocument()))
			{

			}
			
			break;
		}
		case C4DPL_EDITIMAGE:
			return false;
	}

	return false;
}
