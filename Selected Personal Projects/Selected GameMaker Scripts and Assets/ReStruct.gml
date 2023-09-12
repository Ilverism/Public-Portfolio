/*
A complete version of the system I created to enhance GameMaker's object-oriented
programming using lightweight objects and constructors.
Works with static values and constructor scripts.
*/


#macro INST_PRAGMA	-1
#macro INST_PRAGMA_INIT		if (args == INST_PRAGMA) exit

#macro Default:__SHOW_DEBUG__	false
#macro Debugging:__SHOW_DEBUG__ true

#macro __DEFAULT_ARGS_TAG__	"argsDefault"


function set_args(args={}) {
	
	if (args==INST_PRAGMA) { //<-- Directive to ensure static variables can be initialized properly
		exit;
		}

	var this = self;
	var defaultArgs = this[$ __DEFAULT_ARGS_TAG__];
	
	
	var defaultCopy = struct_make_copy(defaultArgs);
	
	
	args = override_struct(defaultCopy, args);
	
		
	var argsNames = variable_struct_get_names(args);
	var argsCount = array_length(argsNames);
	
	var argNameCur;
	var argVal;
	
	
	for(var i = 0 ; i < argsCount ; i++) {	
	
		argNameCur = argsNames[i];
		argVal = (args[$ argNameCur] ?? defaultArgs[$ argNameCur]);
		
		variable_struct_set(this, argNameCur, argVal);
	
		}
		
	return args;
		
	}


function struct_make_type(structTarget, structType=undefined) {

	/*
	- Redefines a given struct to be of a different type
	*/
	
	//No specified struct type
	if (is_undefined(structType)) {
	
		structType = instanceof(structTarget);
	
	    }
		
	//'structTarget' type and 'structType' are both generic
	if (structType == "struct") {
		
		return structTarget;
		
		}
		
		
	//Initialize constructed object
	var assetInd		= asset_get_index(structType);
	var constructFunc	= method(undefined, assetInd);
	if (is_undefined(constructFunc)) {
		
		return structTarget;
		
		}
	
	var structOut = new constructFunc();
	
	
	
	var varNames	  = variable_struct_get_names(structTarget);
	var varNamesCount = array_length(varNames);
	
	var nameCur;
	var valCur;
	for(var i = 0 ; i < varNamesCount ; i++) {
		
		nameCur = varNames[i];
		valCur  = structTarget[$ nameCur];
		
		structOut[$ nameCur] = valCur;
		
	    }

	return structOut;

	}


function struct_make_copy(structTarget=undefined, deepCopy=true) {

	/*
	- Create a copy of a struct
	- Can recursively create a deep copy
	*/
	
	var structOut = {};
	
	
	//ERROR: Target struct does not exist, exit
	if (is_undefined(structTarget)) {
		
		if (__SHOW_DEBUG__) {
			show_debug_message("STRUCT TARGET NOT DEFINED!");
			}
		return structOut;
		
		}		
			
		
	var varNames	  = variable_struct_get_names(structTarget);
	var varNamesCount = array_length(varNames);
	
	var instType;
	
	var varNameCur;
	var varValCur;
	var varValTarget;
	for(var i = 0 ; i < varNamesCount ; i++) {
		
		
		//Get variable value
		varNameCur = varNames[i];
		varValCur  = structTarget[$ varNameCur];
		
		
		//Value is a method
		if (typeof(varValCur) == "method") {
			varValCur = method(structTarget, varValCur);
			
			}
		
		//Target value is another struct, perform deep copy
		else if (deepCopy && is_struct(varValCur)) {
			
			instType = instanceof(varValCur);
						
			//Target struct type is generic
			if (instType == "struct") {
				
				varValCur = struct_make_copy(varValCur, true);
				
				}
			
			//Target struct type is specified
			else {
				
				varValTarget = struct_make_copy(varValCur, true);
				varValCur = struct_make_type(varValTarget, instType);
				
				}
			
			}

		//Copying an array
		else if (typeof(varValCur) == "array") {
			varValCur = array_get_copy(varValCur);
			}
		
		//Add value to output struct
		structOut[$ varNameCur] = varValCur;

		}
	
	//Fetch static variables from the target
	merge_struct(structOut, static_get(structTarget));
	
	return structOut;
	
	}


function merge_struct(structA={}, structB={}) {

	/*
	- Adds values from structB to structA if keys for those values do not already exist
	*/

	var structBNames = variable_struct_get_names(structB);
	var structBNamesCount = array_length(structBNames);
	
	var nameCur;
	for(var i = 0 ; i < structBNamesCount ; i++) {
		
		nameCur = structBNames[i];
		
		structA[$ nameCur] ??= structB[$ nameCur];
		
		}
		
	return structA;

	}
	

function override_struct(structA={}, structB={}) {

	/*
	- Adds values from structB to structA even if keys for those values already exist
	*/
	
	var structBNames = variable_struct_get_names(structB);
	var structBNamesCount = array_length(structBNames);
	
	var nameCur;
	var memberCur;
	for(var i = 0 ; i < structBNamesCount ; i++) {
		
		nameCur = structBNames[i];
		memberCur = structB[$ nameCur];
		
		structA[$ nameCur] = memberCur;

		}
		
	return structA;

	}


function rebind_struct_methods(targetStruct=self, targetScope=targetStruct) {
	
	/*
	- Automatically re-scopes all methods within a given struct to a target
	*/
	
	var structNames = variable_struct_get_names(targetStruct);
	var structNamesCount = array_length(structNames);
	
	var nameCur;
	var memberCur;
	for(var i = 0 ; i < structNamesCount ; i++) {
		
		nameCur = structNames[i];
		memberCur = targetStruct[$ nameCur];
		
		if (typeof(memberCur) == "method") {

			targetStruct[$ nameCur] = method(targetScope, memberCur);
			
			}
		
		}

	}