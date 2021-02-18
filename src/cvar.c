/*
===========================================================================
    Copyright (C) 2010-2013  Ninja and TheKelm of the IceOps-Team
    Copyright (C) 1999-2005 Id Software, Inc.

    This file is part of CoD4X17a-Server source code.

    CoD4X17a-Server source code is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    CoD4X17a-Server source code is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>
===========================================================================
*/
// cvar.c -- dynamic variable tracking

#include "q_shared.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "qcommon_mem.h"
#include "cvar.h"
#include "cmd.h"
#include "cmd_completion.h"
#include "filesystem.h"
#include "sys_main.h"
// nothing outside the Cvar_*() functions should modify these fields!

#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <float.h>

cvar_t		*cvar_vars;
cvar_t		*cvar_cheats;
int		cvar_modifiedFlags;
qboolean	cvar_archivedset = qfalse;
qboolean	cheating_enabled;

static char nullstring[1] = {0};

#define	MAX_CVARS	8192
cvar_t		cvar_indexes[MAX_CVARS];
int			cvar_numIndexes;

#define FILE_HASH_SIZE		512
static	cvar_t*		hashTable[FILE_HASH_SIZE];

typedef struct{
		int integer;
		const char** strings;
}EnumValueStr_t;

typedef struct{
	union{
		float floatval;
		int integer;
		const char* string;
		byte boolean;
		vec4_t vec4;
		vec3_t vec3;
		vec2_t vec2;
		ucolor_t color;
		EnumValueStr_t enumval; /* For Cvar_Register and Cvar_ValidateNewVar only */
	};
}CvarValue;


typedef struct{
	union{
		int imin;
		float fmin;
	};
	union{
		int imax;
		float fmax;
	};
}CvarLimits;


static int Cvar_SetVariant( cvar_t *var, CvarValue value ,qboolean force );
void Cvar_ValueToStr(cvar_t const *cvar, char* bufvalue, int sizevalue, char* bufreset, int sizereset, char* buflatch, int sizelatch);
void Cvar_Set2( const char *var_name, const char *value, qboolean force);

/*
================
return a hash value for the filename
================
*/
static long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

/*
============
Cvar_ValidateString
============
*/
qboolean Cvar_ValidateString( const char *s ) {
	if ( !s ) {
		return qfalse;
	}
	if ( strchr( s, '\\' ) ) {
		return qfalse;
	}
	if ( strchr( s, '\"' ) ) {
		return qfalse;
	}
	if ( strchr( s, ';' ) ) {
		return qfalse;
	}
	return qtrue;
}

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar( const char *var_name ) {
	cvar_t	*var;
	long hash;

	hash = generateHashValue(var_name);
	
	for (var=hashTable[hash] ; var ; var=var->hashNext) {
		if (!Q_stricmp(var_name, var->name)) {
			return var;
		}
	}

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue( const char *var_name ) {
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0.0;
	if(var->type == CVAR_FLOAT)
		return var->value;
	if(var->type == CVAR_INT)
		return (float)var->integer;
	else
		return 0.0;
}


/*
============
Cvar_VariableIntegerValue
============
*/
int Cvar_VariableIntegerValue( const char *var_name ) {
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	if(var->type == CVAR_BOOL)
		return var->boolean;
	if(var->type == CVAR_FLOAT)
		return (int)var->value;
	if(var->type == CVAR_INT)
		return var->integer;
	else
		return 0;
}

/*
============
Cvar_VariableBooleanValue
============
*/
qboolean Cvar_VariableBooleanValue( const char *var_name ) {
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	if(var->type == CVAR_BOOL)
		return var->boolean;
	if(var->type == CVAR_FLOAT)
	{
		if(var->value != 0.0)
			return qfalse;
		else
			return qtrue;
	}
	if(var->type == CVAR_INT)
	{
		if(var->integer == 0)
			return qfalse;
		else
			return qtrue;
	}
	else
		return 0;
}



/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString( const char *var_name ) {
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return "";

	return Cvar_DisplayableValue(var);
}


/*
============
Cvar_VariableStringBuffer
============
*/
void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var) {
		*buffer = 0;
	}
	else {
		Q_strncpyz( buffer, var->string, bufsize );
	}
}

/*

	switch(value)
	{
		case CVAR_BOOL:
			
		case CVAR_FLOAT:

		case CVAR_VEC2:
		case CVAR_VEC3:
		case CVAR_VEC4:
		case CVAR_INT:
		case CVAR_ENUM:
		case CVAR_STRING:
		case CVAR_COLOR:

		default:
			Com_Error( ERR_FATAL, "Cvar_Register: Invalid type" );
	}

*/


/*
============
Cvar_CommandCompletion
============
*/
void	Cvar_CommandCompletion( void(*callback)(const char *s) ) {
	cvar_t		*cvar;
	
	for ( cvar = cvar_vars ; cvar ; cvar = cvar->next ) {
		callback( cvar->name );
	}
}




static qboolean Cvar_ValidateNewVar(const char* var_name, cvarType_t type, CvarValue *value, CvarLimits *limits)
{
	qboolean retval = qtrue;
	int i;
	static const char* enumFailDummy[2];

	if ( !var_name ) {
		Com_Error( ERR_FATAL, "Cvar_Register: NULL parameter" );
		return qfalse;
	}

	if ( !Cvar_ValidateString( var_name ) ) {
		Com_Error( ERR_FATAL, "Cvar_Register: invalid cvar name string: %s\n", var_name );
		return qfalse;
	}

	switch(type)
	{
		case CVAR_BOOL:
			if((*value).boolean > 1)
			{
				(*value).boolean = 0;
				return qfalse;
			}
			return retval; 
		case CVAR_FLOAT:
			if(isnan((*limits).fmax) || isnan((*limits).fmin) || isnan((*value).floatval))
			{
				(*value).floatval = 0.0;
				(*limits).fmax = 0.0;
				(*limits).fmin = 0.0;
				return qfalse;
			}
			if((*limits).fmax < (*limits).fmin)
			{
				(*limits).fmax = (*limits).fmin;
				retval = qfalse;
			}
			if((*value).floatval < (*limits).fmin) 
			{
				(*value).floatval = (*limits).fmin;
				return qfalse;
			}
			if((*value).floatval > (*limits).fmax)
			{
				(*value).floatval = (*limits).fmax;
				return qfalse;
			}
			return retval;
		case CVAR_VEC2:
			for(i = 0; i < 2; i++)
			{
				if(isnan((*value).vec2[i]))
				{
					(*value).vec2[i] = 0.0;
					retval = qfalse;
				}
				if((*value).vec2[i] < (*limits).fmin)
				{
					(*value).vec2[i] = (*limits).fmin;
					retval = qfalse;
				}
				if((*value).vec2[i] > (*limits).fmax)
				{
					(*value).vec2[i] = (*limits).fmax;
					retval = qfalse;
				}
			}
			return retval;
		case CVAR_VEC3:
			for(i = 0; i < 3; i++)
			{
				if(isnan((*value).vec3[i]))
				{
					(*value).vec3[i] = 0.0;
					retval = qfalse;
				}
				if((*value).vec3[i] < (*limits).fmin)
				{
					(*value).vec3[i] = (*limits).fmin;
					retval = qfalse;
				}
				if((*value).vec3[i] > (*limits).fmax)
				{
					(*value).vec3[i] = (*limits).fmax;
					retval = qfalse;
				}
			}
			return retval;
		case CVAR_VEC4:
			for(i = 0; i < 4; i++)
			{
				if(isnan((*value).vec4[i]))
				{
					(*value).vec4[i] = 0.0;
					retval = qfalse;
				}
				if((*value).vec4[i] < (*limits).fmin)
				{
					(*value).vec4[i] = (*limits).fmin;
					retval = qfalse;
				}
				if((*value).vec4[i] > (*limits).fmax)
				{
					(*value).vec4[i] = (*limits).fmax;
					retval = qfalse;
				}
			}
			return retval;
		case CVAR_COLOR:
			return retval;
		case CVAR_ENUM:
			enumFailDummy[0] = "";
			enumFailDummy[1] = NULL;
			if((*value).enumval.strings == NULL)
			{
				(*value).enumval.strings = enumFailDummy;
				(*value).enumval.integer = 0;
				return qfalse;
			}
			for(i = 0; (*value).enumval.strings[i] != NULL && i < (*value).enumval.integer; i++ );
			if(i != (*value).enumval.integer)
			{
				return qfalse;
			}
			return qtrue;
		case CVAR_INT:
			if((*limits).imax < (*limits).imin)
			{
				(*limits).imax = (*limits).imin;
				retval = qfalse;
			}
			if((*value).integer < (*limits).imin) 
			{
				(*value).integer = (*limits).imin;
				return qfalse;
			}
			if((*value).integer > (*limits).imax)
			{
				(*value).integer = (*limits).imax;
				return qfalse;
			}
			return retval;
		case CVAR_STRING:
			/* Always valid */
			if((*value).string == NULL)
			{
				(*value).string = nullstring;
				return qfalse;
			}
			return qtrue;
		default:
			Com_Error( ERR_FATAL, "Cvar_Register: Invalid type" );
			return qfalse;
	}
}
/*

Unlinking is broken


void Cvar_Deallocate( cvar_t *var )
{
	cvar_t *othervar, **back;

	if(var->type == CVAR_STRING)
	{
		if(var->string != NULL)
		{
			Z_Free( var->string );
			var->string = NULL;
		}
		if(var->resetString != NULL)
		{
			Z_Free( var->resetString );
			var->resetString = NULL;
		}
		if(var->latchedString != NULL)
		{
			Z_Free( var->latchedString );
			var->latchedString = NULL;
		}
	}
	if ( var->name ) {
		Z_Free( var->name );
		var->name = NULL;
	}
	if ( var->description ) {
		Z_Free( var->description );
		var->description = NULL;
	}

	back = &cvar_vars;
	while ( 1 ) {
		othervar = *back;
		if ( !othervar ) {
			// command wasn't active
			Com_Error(ERR_FATAL, "Cvar_Deallocate: Tried to unlink a cvar which does not exist");
			return;
		}
		if ( var == othervar ) {
			*back = var->next;
			return;
		}
		back = &othervar->next;
	}
}

*/

void Cvar_FreeStrings( cvar_t *var )
{
	if(var->type == CVAR_STRING)
	{
		if(var->string != NULL)
		{
			if(var->string != nullstring)
				Z_Free( var->string );
			var->string = NULL;
		}
		if(var->resetString != NULL)
		{
			if( var->resetString != nullstring)
				Z_Free( var->resetString );
			var->resetString = NULL;
		}
		if(var->latchedString != NULL )
		{
			if(var->latchedString != nullstring)
				Z_Free( var->latchedString );
			var->latchedString = NULL;
		}
	}
	if ( var->name ) {
		Z_Free( var->name );
		var->name = NULL;
	}
	if ( var->description ) {
		if(var->description != nullstring)
			Z_Free( var->description );
		var->description = NULL;
	}
}

/*
============
Cvar_Register

If the variable already exists, the value will not be set unless CVAR_ROM
The flags will be or'ed in if the variable exists.
============
*/


static cvar_t *Cvar_Register(const char* var_name, cvarType_t type, unsigned short flags, CvarValue value, CvarLimits limits, const char *description)
{
	cvar_t *var;
	qboolean isvalid;
	long	hash;
	cvar_t* safenext;
	cvar_t* safehashNext;
	char latchedStr[8192];

	isvalid = Cvar_ValidateNewVar(var_name, type, &value, &limits);

	if(isvalid == qfalse)
	{
		Com_Error( ERR_FATAL, "Cvar_Register: invalid cvar value or type for: %s\n", var_name );
		return NULL;
	}

	var = Cvar_FindVar(var_name);

	if( var && ( ( (flags & CVAR_ROM) || (flags & CVAR_CHEAT && !cheating_enabled)) && var->flags & CVAR_USER_CREATED))
	{
		Cvar_FreeStrings( var );
	}else if ( var ) {
		/*
		This is Reregister
		if the C code is now specifying a variable that the user already
		set a value for, take the new value as the reset value
		*/
		if(var->type != type && !(var->flags & CVAR_USER_CREATED))
		{
			Com_Error( ERR_FATAL, "Cvar_Register: Tried to reregister an already registered Cvar \'%s\' as a different type\n", var_name );
			return NULL;
		}
		/* Get the old value from string var */
		/* We change the type of a user created Cvar. Free the strings and keep the current value for later eval */

		Cvar_ValueToStr(var, NULL, 0, NULL, 0, latchedStr, sizeof(latchedStr));

		if(var->type == CVAR_STRING)
		{
			if(var->resetString && var->resetString != nullstring)
				Z_Free(var->resetString);
			if(var->latchedString && var->latchedString != nullstring)
				Z_Free(var->latchedString);
			if(var->string && var->string != nullstring)
				Z_Free(var->string);
			var->string = var->latchedString = var->resetString = NULL;
		}

		if ( ( var->flags & CVAR_USER_CREATED ) && !( flags & CVAR_USER_CREATED ) )
		{
			var->flags &= ~CVAR_USER_CREATED;
			cvar_modifiedFlags |= flags;
		}
		/* Apply the new reset values and limits */
		switch(type)
		{
			case CVAR_BOOL:
				var->resetBoolean = value.boolean;
				break;
			case CVAR_FLOAT:
				var->resetFloatval = value.floatval;
				var->fmin = limits.fmin;
				var->fmax = limits.fmax;
				break;
			case CVAR_VEC2:
			case CVAR_VEC3:
			case CVAR_VEC4:
				var->fmin = limits.fmin;
				var->fmax = limits.fmax;
				break;
			case CVAR_COLOR:
				var->resetColor = value.color;
				break;
			case CVAR_ENUM:
				var->resetInteger = value.enumval.integer;
				var->imin = 0;
				var->enumStr = value.enumval.strings;
				break;
			case CVAR_INT:
				var->resetInteger = value.integer;
				var->imin = limits.imin;
				var->imax = limits.imax;
				break;
			case CVAR_STRING:
				var->resetString = CopyString( value.string );
		}
		/* Apply the new description */
		if(description && description[0])
		{
			if ( var->description && var->description != nullstring)
			{
				Z_Free( var->description );
			}
			var->description = CopyString(description);
		}
		else
			var->description = nullstring;

		var->flags |= flags;
		var->type = type;

		// Take the latched value now
		Cvar_Set2( var_name, latchedStr, qtrue );
		return var;
	}
	if(!var)
	{
		//Doing some stuff for a new var
		if ( cvar_numIndexes >= MAX_CVARS ) {
			Com_Error( ERR_FATAL, "MAX_CVARS" );
			return NULL;
		}

		var = &cvar_indexes[cvar_numIndexes];
		cvar_numIndexes++;

		Com_Memset(var, 0, sizeof(cvar_t));
		var->name = CopyString (var_name);

		// link the variable in
		var->next = cvar_vars;
		cvar_vars = var;
		hash = generateHashValue(var_name);
		var->hashNext = hashTable[hash];
		hashTable[hash] = var;

	}else{
		safenext = var->next;
		safehashNext = var->hashNext;

		Com_Memset(var, 0, sizeof(cvar_t));
		var->name = CopyString (var_name);

		// variable is already linked in
		var->next = safenext;
		var->hashNext = safehashNext;
	}

	if(description && description[0])
		var->description = CopyString(description);
	else
		var->description = nullstring;

	var->flags = flags;
	var->type = type;
	var->modified = qtrue; /* Is this true ?*/

	switch(type)
	{
		case CVAR_BOOL:
			var->boolean = value.boolean;
			var->resetBoolean = value.boolean;
			var->latchedBoolean = value.boolean;
			break;
		case CVAR_FLOAT:
			var->floatval = value.floatval;
			var->resetFloatval = value.floatval;
			var->latchedFloatval = value.floatval;
			var->fmin = limits.fmin;
			var->fmax = limits.fmax;
			break;
		case CVAR_VEC2:
		case CVAR_VEC3:
		case CVAR_VEC4:
			var->fmin = limits.fmin;
			var->fmax = limits.fmax;
			memcpy(var->vec4, value.vec4, sizeof(var->vec4));
			memcpy(var->resetVec4, value.vec4, sizeof(var->resetVec4));
			memcpy(var->latchedVec4, value.vec4, sizeof(var->latchedVec4));
			break;
		case CVAR_COLOR:
			var->color = value.color;
			var->resetColor = value.color;
			var->latchedColor = value.color;
			break;
		case CVAR_ENUM:
			var->integer = value.enumval.integer;
			var->resetInteger = value.enumval.integer;
			var->latchedInteger = value.enumval.integer;
			var->imin = 0;
			var->enumStr = value.enumval.strings;
			break;
		case CVAR_INT:
			var->integer = value.integer;
			var->resetInteger = value.integer;
			var->latchedInteger = value.integer;
			var->imin = limits.imin;
			var->imax = limits.imax;
			break;
		case CVAR_STRING:
			var->string = CopyString( value.string );
			var->resetString = CopyString( value.string );
			var->latchedString = CopyString( value.string );
	}
	cvar_modifiedFlags |= var->flags;
	return var;
}



/*
============
Cvar_SetVariant
============
*/
static int Cvar_SetVariant( cvar_t *var, CvarValue value ,qboolean force ) {

	int latched = 0;
	int i;

	if (!var)
		return 0;



	if(force == qfalse)
	{
		if(var->flags & CVAR_ROM)
		{
			Com_Printf ("%s is read only.\n", var->name);
			return 0;
		}

		if (var->flags & CVAR_INIT)
		{
			Com_Printf ("%s is write protected.\n", var->name);
			return 0;
		}

		if ( (var->flags & CVAR_CHEAT) && !cvar_cheats->boolean )
		{
			Com_Printf ("%s is cheat protected.\n", var->name);
			return 0;
		}

		if(var->flags & CVAR_LATCH)
		{
			Com_Printf ("%s will be changed upon restarting.\n", var->name);
			latched = 1;
		}
	}

	switch(var->type){
		case CVAR_BOOL:
			if((var->boolean && value.boolean) || (!var->boolean && !value.boolean))
				return 0;

			if(value.boolean){
				if(!latched) var->boolean = 1;
				var->latchedBoolean = 1;
			}else{
				if(!latched) var->boolean = 0;
				var->latchedBoolean = 0;
			}
			break;
		case CVAR_FLOAT:
			if(var->floatval == value.floatval)
				return 0;

			if(isnan(value.floatval))
				return -1;

			if(value.floatval < var->fmin || value.floatval > var->fmax)
			{
				Com_Printf ("\'%g\' is not a valid value for cvar '%s'\n", value.floatval, var->name);
				Com_Printf ("Domain is any float in range between \'%g\' and \'%g\'\n", var->fmin, var->fmax);
				return -1;
			}
			if(!latched) var->floatval = value.floatval;
			var->latchedFloatval = value.floatval;
			break;
		case CVAR_VEC2:
			for(i = 0; i < 2; i++)
			{
				if(isnan(value.vec2[i]))
					return -1;
				if(value.vec2[i] < var->fmin || value.vec2[i] > var->fmax)
				{
					Com_Printf ("\'%g\' is not a valid value for cvar '%s'\n", value.vec2[i], var->name);
					Com_Printf ("Domain is any float in range between \'%g\' and \'%g\'\n", var->fmin, var->fmax);
					return -1;
				}
			}
			for(i = 0; i < 2; i++)
			{
				if(!latched) var->vec2[i] = value.vec2[i];
				var->latchedVec2[i] = value.vec2[i];
			}
			break;
		case CVAR_VEC3:
			for(i = 0; i < 3; i++)
			{
				if(isnan(value.vec3[i]))
					return -1;
				if(value.vec3[i] < var->fmin || value.vec3[i] > var->fmax)
				{
					Com_Printf ("\'%g\' is not a valid value for cvar '%s'\n", value.vec3[i], var->name);
					Com_Printf ("Domain is any float in range between \'%g\' and \'%g\'\n", var->fmin, var->fmax);
					return -1;
				}
			}
			for(i = 0; i < 3; i++)
			{
				if(!latched) var->vec3[i] = value.vec3[i];
				var->latchedVec3[i] = value.vec3[i];
			}
			break;
		case CVAR_VEC4:
			for(i = 0; i < 4; i++)
			{
				if(isnan(value.vec4[i]))
					return -1;
				if(value.vec4[i] < var->fmin || value.vec4[i] > var->fmax)
				{
					Com_Printf ("\'%g\' is not a valid value for cvar '%s'\n", value.vec4[i], var->name);
					Com_Printf ("Domain is any float in range between \'%g\' and \'%g\'\n", var->fmin, var->fmax);
					return -1;
				}
			}
			for(i = 0; i < 4; i++)
			{
				if(!latched) var->vec4[i] = value.vec4[i];
				var->latchedVec4[i] = value.vec4[i];
			}
			break;
		case CVAR_COLOR:
			if(!latched) var->color = value.color;
			var->latchedColor = value.color;
			break;
		case CVAR_ENUM:
			if(var->integer == value.integer)
				return 0;
			if(var->enumStr == NULL)
				return -1;
			for(i = 0; var->enumStr[i] != NULL && i != value.integer; i++ );
			if(var->enumStr[i] == NULL)
			{
				Com_Printf ("\'%d\' is not a valid value for cvar '%s'\n", value.integer, var->name);
				Com_Printf ("  Domain is one of the following:\n");
				for(i = 0; var->enumStr[i] != NULL; i++ )
					Com_Printf ("   %d: %s\n", var->enumStr[i]);
				return -1;
			}
			if(!latched) var->integer = value.integer;
			var->latchedInteger = value.integer;
			break;
		case CVAR_INT:
			if(var->integer == value.integer)
				return 0;

			if(value.integer < var->imin || value.integer > var->imax)
			{
				Com_Printf ("\'%d\' is not a valid value for cvar '%s'\n", value.integer, var->name);
				Com_Printf ("  Domain is any integer in range between \'%d\' and \'%d\'\n", var->imin, var->imax);
				return -1;
			}
			if(!latched) var->integer = value.integer;
			var->latchedInteger = value.integer;
			break;
		case CVAR_STRING:
			if(!value.string)
				return 0;

			if(var->string && !Q_stricmp(var->string, value.string))
				return 0;

			if(!latched)
			{
				if(var->string && var->string != nullstring)
					Z_Free(var->string);

				var->string = CopyString( value.string );
			}

			if(var->latchedString && var->latchedString != nullstring)
				Z_Free(var->latchedString);

			var->latchedString = CopyString( value.string );
	}
	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |= var->flags;
	var->modified = qtrue;
	return 1;
}


/*
============
Cvar_Set2
============
*/
void Cvar_Set2( const char *var_name, const char *valueStr, qboolean force ) {
	cvar_t	*var;

	int integer;
	float floatval;
	CvarValue value;
	CvarLimits limits;
	vec4_t colorConv;

	if ( !Cvar_ValidateString( var_name ) ) {
		Com_Printf("invalid cvar name string: %s\n", var_name );
		return;
	}

	if(valueStr == NULL)
		return;

	var = Cvar_FindVar(var_name);
	if(var)
	{
		if(!force){
			if(var->flags & CVAR_ROM)
			{
				Com_Printf ("%s is read only.\n", var->name);
				return;
			}
			if (var->flags & CVAR_INIT)
			{
				Com_Printf ("%s is write protected.\n", var->name);
				return;
			}
		}
		/* Get the type */
		switch(var->type){
		case CVAR_BOOL:
			if(isInteger(valueStr, 0)){
				integer = atoi(valueStr);
				value.boolean = integer;
				Cvar_SetVariant( var, value, force );
			}else if(!Q_stricmp(valueStr, "true")){
				integer = 1;
				value.boolean = integer;
				Cvar_SetVariant( var, value, force );
			}else if(!Q_stricmp(valueStr, "false")){
				integer = 0;
				value.boolean = integer;
				Cvar_SetVariant( var, value, force );
			}
			return;
		case CVAR_FLOAT:
			if(isFloat(valueStr, 0))
			{
				floatval = atof(valueStr);
				value.floatval = floatval;
				Cvar_SetVariant( var, value, force );
			}
			return;
		case CVAR_VEC2:
			if(isVector(valueStr, 0, 2))
			{
				strToVect(valueStr ,value.vec2, 2);
				Cvar_SetVariant( var, value, force );
			}
			return;
		case CVAR_VEC3:
			if(isVector(valueStr, 0, 3))
			{
				strToVect(valueStr ,value.vec3, 3);
				Cvar_SetVariant( var, value, force );
			}
			return;
		case CVAR_COLOR:
			if(isVector(valueStr, 0, 4))
			{
				strToVect(valueStr ,colorConv, 4);
				value.color.alpha = (byte)((float)0xff * colorConv[3]);
			}else if(isVector(valueStr, 0, 3)){
				strToVect(valueStr ,colorConv, 3);
				value.color.alpha = 0x0;
			}else{
				return;
			}
			value.color.red = (byte)((float)0xff * colorConv[0]);
			value.color.green = (byte)((float)0xff * colorConv[1]);
			value.color.blue = (byte)((float)0xff * colorConv[2]);
			Cvar_SetVariant( var, value, force );
			return;
		case CVAR_VEC4:
			if(isVector(valueStr, 0, 4))
			{
				strToVect(valueStr ,value.vec4, 4);
				Cvar_SetVariant( var, value, force );
			}
			return;
		case CVAR_ENUM:
		case CVAR_INT:
			if(isInteger(valueStr, 0))
			{
				integer = atoi(valueStr);
				value.integer = integer;
				Cvar_SetVariant( var, value, force );
			}
			return;
		case CVAR_STRING:
			value.string = valueStr;
			Cvar_SetVariant( var, value, force );
			return;
		}
	}
	/*
	Seems like we want to create a new cvar. Only string type is supported
	*/
	value.string = valueStr;
	limits.imin = 0;
	limits.imax = 0;
	// create it
	if ( !force ) {
		Cvar_Register( var_name, CVAR_STRING, CVAR_USER_CREATED, value, limits, "");
	} else {
		Cvar_Register( var_name, CVAR_STRING, 0, value, limits, "");
	}
}

/*
============
Cvar_SetWithType
============
*/
void Cvar_SetWithType( const char *var_name, CvarValue value, cvarType_t type, qboolean force ) {
	cvar_t	*var;

	CvarLimits limits;

	if ( !Cvar_ValidateString( var_name ) ) {
		Com_Printf("invalid cvar name string: %s\n", var_name );
		return;
	}

	var = Cvar_FindVar(var_name);

	if(var)
	{
		if(type == var->type)
		{
			Cvar_SetVariant( var, value, force );
			return;
		}else if(force && !(var->flags & CVAR_USER_CREATED)){
			Com_Error(ERR_FATAL, "Cvar_SetWithType: Existing Cvar %s has a different type. Old type: %d New type: %d", var->name, var->type, type);
			return;
		}
	}

	limits.imin = 0;
	limits.imax = 0;
	if(type == CVAR_INT)
	{
		limits.imin = INT_MIN;
		limits.imax = INT_MAX;
	}else if(type == CVAR_FLOAT){
		limits.fmin = -FLT_MAX;
		limits.fmax = +FLT_MAX;
	}
	// create it
	if ( !force ) {
		Cvar_Register( var_name, CVAR_STRING, CVAR_USER_CREATED, value, limits, "External Cvar");
	} else {
		Cvar_Register( var_name, CVAR_STRING, 0, value, limits, "External Cvar");
	}
}




/*
============
Cvar_Set
============
*/
void Cvar_Set( const char *var_name, const char *value) {
	Cvar_Set2 (var_name, value, qtrue);
}

/*
============
Cvar_SetAllowCheat
============
*/
void Cvar_SetAllowCheatOnly( const char *var_name, const char *value) {

	cvar_t* var;

	var = Cvar_FindVar(var_name);
	if(var && var->flags & CVAR_CHEAT)
	{
		var->flags &= ~CVAR_CHEAT;
		Cvar_Set2 (var_name, value, qfalse);
		var->flags |= CVAR_CHEAT;
	}else{
		Cvar_Set2 (var_name, value, qfalse);
	}
}

void Cvar_SetStringByName( const char* var_name, const char* value) {
	CvarValue cval;
	cval.string = value;
	Cvar_SetWithType( var_name, cval, CVAR_STRING, qtrue );
}


/*
============
Cvar_SetLatched

============
*/
void Cvar_SetLatched( const char *var_name, const char *value) {
	Cvar_Set2 (var_name, value, qfalse);
}

/*
============
Cvar_SetFloat
============
*/
void Cvar_SetFloat( cvar_t* cvar, float value) {
	CvarValue cval;
	cval.floatval = value;
	Cvar_SetVariant( cvar, cval , qtrue);
}

void Cvar_SetFloatByName( const char* var_name, float value) {
	CvarValue cval;
	cval.floatval = value;
	Cvar_SetWithType( var_name, cval, CVAR_FLOAT, qtrue );
}

/*
============
Cvar_SetInt
============
*/
void Cvar_SetInt( cvar_t* cvar, int value){
	CvarValue cval;
	cval.integer = value;
	Cvar_SetVariant( cvar, cval , qtrue);
}

void Cvar_SetIntByName( const char* var_name, int value){
	CvarValue cval;
	cval.integer = value;
	Cvar_SetWithType( var_name, cval, CVAR_INT, qtrue );
}

/*
============
Cvar_SetEnum
============
*/
void Cvar_SetEnum( cvar_t* cvar, int value){
	CvarValue cval;
	cval.integer = value;
	Cvar_SetVariant( cvar, cval , qtrue);
}


/*
============
Cvar_SetBool
============
*/
void Cvar_SetBool( cvar_t* cvar, qboolean value){
	CvarValue cval;
	cval.boolean = value;
	Cvar_SetVariant( cvar, cval , qtrue);
}

void Cvar_SetBoolByName( const char* var_name, qboolean value){
	CvarValue cval;
	cval.boolean = value;
	Cvar_SetWithType( var_name, cval, CVAR_BOOL, qtrue );
}


/*
============
Cvar_SetString
============
*/
void Cvar_SetString( cvar_t* cvar, const char* value){
	CvarValue cval;
	cval.string = value;
	Cvar_SetVariant( cvar, cval , qtrue);
}

/*
============
Cvar_SetVec2
============
*/
void Cvar_SetVec2( cvar_t* cvar, float x, float y){
	CvarValue cval;
	cval.vec2[0] = x;
	cval.vec2[1] = y;
	Cvar_SetVariant( cvar, cval , qtrue);
}

/*
============
Cvar_SetVec3
============
*/
void Cvar_SetVec3( cvar_t* cvar, float x, float y, float z){
	CvarValue cval;
	cval.vec3[0] = x;
	cval.vec3[1] = y;
	cval.vec3[2] = z;
	Cvar_SetVariant( cvar, cval , qtrue);
}

/*
============
Cvar_SetVec4
============
*/
void Cvar_SetVec4( cvar_t* cvar, float x, float y, float z, float imag){
	CvarValue cval;
	cval.vec4[0] = x;
	cval.vec4[1] = y;
	cval.vec4[2] = z;
	cval.vec4[3] = imag;
	Cvar_SetVariant( cvar, cval , qtrue);
}

/*
============
Cvar_SetColor
============
*/
void Cvar_SetColor( cvar_t* cvar, float r, float g, float b, float alpha){
	CvarValue cval;

	cval.color.red = (byte)((float)0xff * r);
	cval.color.green = (byte)((float)0xff * g);
	cval.color.blue = (byte)((float)0xff * b);
	cval.color.alpha = (byte)((float)0xff * alpha);

	Cvar_SetVariant( cvar, cval , qtrue);
}


/*
============
Cvar_ResetVar
============
*/
void Cvar_ResetVar( cvar_t* var ) {

	CvarValue cval;

	if(!var)
		return;

	switch(var->type)
	{
		case CVAR_BOOL:
			cval.boolean = var->resetBoolean;
			break;
		case CVAR_FLOAT:
			cval.floatval = var->resetFloatval;
			break;
		case CVAR_VEC2:
		case CVAR_VEC3:
		case CVAR_VEC4:
			cval.vec4[0] = var->resetVec4[0];
			cval.vec4[1] = var->resetVec4[1];
			cval.vec4[2] = var->resetVec4[2];
			cval.vec4[3] = var->resetVec4[3];
			break;
		case CVAR_COLOR:
			cval.color = var->resetColor;
			break;
		case CVAR_INT:
		case CVAR_ENUM:
			cval.integer = var->resetInteger;
			break;
		case CVAR_STRING:
			cval.string = var->resetString;
			break;
		default:
			Com_Error( ERR_FATAL, "Cvar_Register: Invalid type" );
	}
	Cvar_SetVariant( var, cval , qtrue);
}


/*
============
Cvar_Reset
============
*/
void Cvar_Reset( const char *var_name )
{

	cvar_t *var;
	var = Cvar_FindVar(var_name);

	Cvar_ResetVar( var );

}



/*
============
Cvar_SetCheatState

Any testing variables will be reset to the safe values
============
*/
void Cvar_SetCheatState( void ) {

    cvar_t	*var;
    CvarValue cval;
	// set all default vars to the safe value
    for ( var = cvar_vars ; var ; var = var->next ) {
	if ( var->flags & CVAR_CHEAT ) {
		// the CVAR_LATCHED|CVAR_CHEAT vars might escape the reset here 
		// because of a different var->latchedString
		switch(var->type)
		{
		case CVAR_BOOL:
			cval.boolean = var->resetBoolean;
			break;
		case CVAR_FLOAT:
			cval.floatval = var->resetFloatval;
			break;
		case CVAR_VEC2:
		case CVAR_VEC3:
		case CVAR_VEC4:
			cval.vec4[0] = var->resetVec4[0];
			cval.vec4[1] = var->resetVec4[1];
			cval.vec4[2] = var->resetVec4[2];
			cval.vec4[3] = var->resetVec4[3];
			break;
		case CVAR_COLOR:
			cval.color = var->resetColor;
			break;
		case CVAR_INT:
		case CVAR_ENUM:
			cval.integer = var->resetInteger;
			break;
		case CVAR_STRING:
			cval.string = var->resetString;
			break;
		default:
			continue;
		}
		if(var->flags & CVAR_LATCH)
			Cvar_SetVariant( var, cval , qfalse); /* Don't force latched Cvars as this can be dangerous */
		else
			Cvar_SetVariant( var, cval , qtrue);
	}
    }
}


void Cvar_ValueToStr(cvar_t const *cvar, char* bufvalue, int sizevalue, char* bufreset, int sizereset, char* buflatch, int sizelatch)
{
	int i;

	if(!cvar)
	{
		if(buflatch) buflatch[0] = '\0';
		if(bufreset) bufreset[0] = '\0';
		if(bufvalue) bufvalue[0] = '\0';
		return;
	}

	switch(cvar->type)
		{
		case CVAR_BOOL:
			if(bufvalue)
			{
				if(cvar->boolean)
//					Com_sprintf(bufvalue, sizevalue, "true");
					Com_sprintf(bufvalue, sizevalue, "1");
				else
//					Com_sprintf(bufvalue, sizevalue, "false");
					Com_sprintf(bufvalue, sizevalue, "0");
			}
			if(bufreset)
			{
				if(cvar->resetBoolean)
//					Com_sprintf(bufreset, sizereset, "true");
					Com_sprintf(bufreset, sizereset, "1");
				else
//					Com_sprintf(bufreset, sizereset, "false");
					Com_sprintf(bufreset, sizereset, "0");
			}
			if(buflatch)
			{
				if(cvar->latchedBoolean)
//					Com_sprintf(buflatch, sizelatch, "true");
					Com_sprintf(buflatch, sizelatch, "1");
				else
//					Com_sprintf(buflatch, sizelatch, "false");
					Com_sprintf(buflatch, sizelatch, "0");
			}
			return;
		case CVAR_FLOAT:
			if(bufvalue) Com_sprintf(bufvalue, sizevalue, "%g", cvar->floatval);
			if(bufreset) Com_sprintf(bufreset, sizereset, "%g", cvar->resetFloatval);
			if(buflatch) Com_sprintf(buflatch, sizelatch, "%g", cvar->latchedFloatval);
			return;
		case CVAR_VEC2:
			if(bufvalue) Com_sprintf(bufvalue, sizevalue, "%g %g", cvar->vec2[0], cvar->vec2[1]);
			if(bufreset) Com_sprintf(bufreset, sizereset, "%g %g", cvar->resetVec2[0], cvar->resetVec2[1]);
			if(buflatch) Com_sprintf(buflatch, sizelatch, "%g %g", cvar->latchedVec2[0], cvar->latchedVec2[1]);
			return;
		case CVAR_VEC3:
			if(bufvalue) Com_sprintf(bufvalue, sizevalue, "%g %g %g", cvar->vec3[0], cvar->vec3[1], cvar->vec3[2]);
			if(bufreset) Com_sprintf(bufreset, sizereset, "%g %g %g", cvar->resetVec3[0], cvar->resetVec3[1], cvar->resetVec3[2]);
			if(buflatch) Com_sprintf(buflatch, sizelatch, "%g %g %g", cvar->latchedVec3[0], cvar->latchedVec3[1], cvar->latchedVec3[2]);
			return;
		case CVAR_VEC4:
			if(bufvalue) Com_sprintf(bufvalue, sizevalue, "%g %g %g %g", cvar->vec4[0], cvar->vec4[1], cvar->vec4[2], cvar->vec4[3]);
			if(bufreset) Com_sprintf(bufreset, sizereset, "%g %g %g %g", cvar->resetVec4[0], cvar->resetVec4[1], cvar->resetVec4[2], cvar->resetVec4[3]);
			if(buflatch) Com_sprintf(buflatch, sizelatch, "%g %g %g %g", cvar->latchedVec4[0], cvar->latchedVec4[1], cvar->latchedVec4[2], cvar->latchedVec4[3]);
			return;
		case CVAR_COLOR:
			if(bufvalue) Com_sprintf(bufvalue, sizevalue, "%.3g %.3g %.3g %.3g", (float)cvar->color.red / (float)0xff, (float)cvar->color.green / (float)0xff, (float)cvar->color.blue / (float)0xff, (float)cvar->color.alpha / (float)0xff);
			if(bufreset) Com_sprintf(bufreset, sizereset, "%.3g %.3g %.3g %.3g", (float)cvar->resetColor.red / (float)0xff, (float)cvar->resetColor.green / (float)0xff, (float)cvar->resetColor.blue / (float)0xff, (float)cvar->resetColor.alpha / (float)0xff);
			if(buflatch) Com_sprintf(buflatch, sizelatch, "%.3g %.3g %.3g %.3g", (float)cvar->latchedColor.red / (float)0xff, (float)cvar->latchedColor.green / (float)0xff, (float)cvar->latchedColor.blue / (float)0xff, (float)cvar->latchedColor.alpha / (float)0xff);
			return;
		case CVAR_INT:
			if(bufvalue) Com_sprintf(bufvalue, sizevalue, "%d", cvar->integer);
			if(bufreset) Com_sprintf(bufreset, sizereset, "%d", cvar->resetInteger);
			if(buflatch) Com_sprintf(buflatch, sizelatch, "%d", cvar->latchedInteger);
			return;
		case CVAR_STRING:
			if(bufvalue) Com_sprintf(bufvalue, sizevalue, "%s", cvar->string);
			if(bufreset) Com_sprintf(bufreset, sizereset, "%s", cvar->resetString);
			if(buflatch) Com_sprintf(buflatch, sizelatch, "%s", cvar->latchedString);
			return;
		case CVAR_ENUM:
			if(cvar->enumStr)
			{
				if(bufvalue)
				{
					for(i = 0; cvar->enumStr[i] != NULL && i != cvar->integer; i++ );
					if(cvar->enumStr[i] != NULL)
						Com_sprintf(bufvalue, sizevalue, "%s", cvar->enumStr[i]);
					else
						Com_sprintf(bufvalue, sizevalue, "Out of Range value");
				}
				if(bufreset)
				{
					for(i = 0; cvar->enumStr[i] != NULL && i != cvar->resetInteger; i++ );
					if(cvar->enumStr[i] != NULL)
						Com_sprintf(bufreset, sizereset, "%s", cvar->enumStr[i]);
					else
						Com_sprintf(bufreset, sizereset, "Out of Range value");
				}
				if(buflatch)
				{
					for(i = 0; cvar->enumStr[i] != NULL && i != cvar->latchedInteger; i++ );
					if(cvar->enumStr[i] != NULL)
						Com_sprintf(buflatch, sizelatch, "%s", cvar->enumStr[i]);
					else
						Com_sprintf(buflatch, sizelatch, "Out of Range value");
				}
			}
			return;
		default:
			if(bufvalue) bufvalue[0] = '\0';
			if(bufreset) bufreset[0] = '\0';
			if(buflatch) buflatch[0] = '\0';
			return;
	}


}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command( void ) {
	cvar_t			*v;

	char value[1024];
	char reset[1024];
	char latch[1024];

	// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v) {
		return qfalse;
	}

	// perform a variable print or set
	if ( Cmd_Argc() == 1 ) {
		Cvar_ValueToStr(v, value, sizeof(value), reset, sizeof(reset), latch, sizeof(latch));

		if(v->description != NULL)
			Com_Printf ("\"%s\" is: \"%s" S_COLOR_WHITE "\" default: \"%s" S_COLOR_WHITE "\" info: \"%s" S_COLOR_WHITE "\"\n", v->name, value, reset, v->description );
		else
			Com_Printf ("\"%s\" is: \"%s" S_COLOR_WHITE "\" default: \"%s" S_COLOR_WHITE "\"\n", v->name, value, reset );

		if ( Q_stricmp(value, latch) ) {
			Com_Printf( "latched: \"%s\"\n", latch );
		}
		return qtrue;
	}

	// set the value if forcing isn't required
	Cvar_Set2 (v->name, Cmd_Argv(1), qfalse);
	return qtrue;
}


/*
============
Cvar_Toggle_f

Toggles a cvar for easy single key binding
============
*/
void Cvar_Toggle_f( void ) {
	int		v;
	mvabuf;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("usage: toggle <variable>\n");
		return;
	}

	v = Cvar_VariableValue( Cmd_Argv( 1 ) );
	v = !v;

	Cvar_Set2 (Cmd_Argv(1), va("%i", v), qfalse);
}


/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console, even if they
weren't declared in C code.
============
*/
void Cvar_Set_f( void ) {
	int		i, c, l, len;
	char	combined[8192];

	c = Cmd_Argc();
	if ( c < 3 ) {
		Com_Printf ("usage: set <variable> <value>\n");
		return;
	}

	combined[0] = 0;
	l = 0;
	for ( i = 2 ; i < c ; i++ ) {
		len = strlen ( Cmd_Argv( i ) + 1 );
		if ( l + len >= sizeof(combined) - 2 ) {
			break;
		}
		strcat( combined, Cmd_Argv( i ) );
		if ( i != c-1 ) {
			strcat( combined, " " );
		}
		l += len;
	}
	Cvar_Set2 (Cmd_Argv(1), combined, qfalse);
}


/*
============
Cvar_SetFromCvar_f

Allows setting and defining of arbitrary cvars from console, even if they
weren't declared in C code.
============
*/
void Cvar_SetFromCvar_f( void ) {
	int c;
	cvar_t* v;
	char value[8192];

	c = Cmd_Argc();
	if ( c < 3 ) {
		Com_Printf ("usage: setfromcvar <dest_cvar> <source_cvar>\n");
		return;
	}

	v = Cvar_FindVar( Cmd_Argv( 2 ) );

	if(!v)
	{
		Com_Printf ("cvar %s does not exist\n", Cmd_Argv( 2 ));
		return;
	}

	Cvar_ValueToStr(v, value, sizeof(value), NULL, 0, NULL, 0);

	Cvar_Set2 (Cmd_Argv(1), value, qfalse);
}

/*
============
Cvar_SetToTime_f

Allows setting and defining of arbitrary cvars from console, even if they
weren't declared in C code.
============
*/
void Cvar_SetToTime_f( void ) {
	int c, t;
	char value[32];

	c = Cmd_Argc();
	if ( c != 2 ) {
		Com_Printf ("usage: setcvartotime <variablename>\n");
		return;
	}

	t = Sys_Milliseconds();

	Com_sprintf(value, sizeof(value), "%d", t);

	Cvar_Set2 (Cmd_Argv(1), value, qfalse);
}

/*
============
Cvar_SetU_f

As Cvar_Set, but also flags it as userinfo
============
*/
void Cvar_SetU_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf ("usage: setu <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v ) {
		return;
	}
	v->flags |= CVAR_USERINFO;
}

/*
============
Cvar_SetS_f

As Cvar_Set, but also flags it as userinfo
============
*/
void Cvar_SetS_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf ("usage: sets <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v ) {
		return;
	}
	v->flags |= CVAR_SERVERINFO;
}

/*
============
Cvar_SetA_f

As Cvar_Set, but also flags it as archived
============
*/
void Cvar_SetA_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf ("usage: seta <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v ) {
		return;
	}
	v->flags |= CVAR_ARCHIVE;
	cvar_archivedset = qtrue;
}

/*
============
Cvar_Reset_f
============
*/
void Cvar_Reset_f( void ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("usage: reset <variable>\n");
		return;
	}
	Cvar_Reset( Cmd_Argv( 1 ) );
}

/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to qtrue.
============
*/
void Cvar_WriteVariables(fileHandle_t f)
{
	cvar_t	*var;
	char	bufferval[8192];
	char	bufferlatch[8192];
	char	buffer[8192];

	for (var = cvar_vars; var; var = var->next)
	{
		if( var->flags & CVAR_ARCHIVE ) {

			Cvar_ValueToStr(var, bufferval, sizeof(bufferval), NULL, 0, bufferlatch, sizeof(bufferlatch));

			// write the latched value, even if it hasn't taken effect yet
			if ( bufferlatch[0] ) {
				if( strlen( var->name ) + strlen( bufferlatch ) + 10 > sizeof( buffer ) ) {
					Com_Printf( S_COLOR_YELLOW "WARNING: value of variable "
							"\"%s\" too long to write to file\n", var->name );
					continue;
				}
				Com_sprintf (buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, bufferlatch);
			} else {
				if( strlen( var->name ) + strlen( bufferval ) + 10 > sizeof( buffer ) ) {
					Com_Printf( S_COLOR_YELLOW "WARNING: value of variable "
							"\"%s\" too long to write to file\n", var->name );
					continue;
				}
				Com_sprintf (buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, bufferval);
			}
			FS_Write( buffer, strlen( buffer ), f );
		}
	}
}



/*
============
Cvar_List_f
============
*/
void Cvar_List_f( void ) {
	cvar_t	*var;
	int	i;
	char	*match;
	char value[1024];

	if ( Cmd_Argc() > 1 ) {
		match = Cmd_Argv( 1 );
		Com_Printf("Displaying all cvars starting with: %s\n", match);
	} else {
		match = NULL;
	}
	Com_Printf("====================================== Cvar List ======================================\n");

	i = 0;
	for (var = cvar_vars ; var ; var = var->next, i++)
	{
		if (match && !Com_Filter(match, var->name, qfalse)) continue;

		if (var->flags & CVAR_SERVERINFO) {
			Com_Printf("S");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_USERINFO) {
			Com_Printf("U");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_ROM) {
			Com_Printf("R");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_INIT) {
			Com_Printf("I");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_ARCHIVE) {
			Com_Printf("A");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_LATCH) {
			Com_Printf("L");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_CHEAT) {
			Com_Printf("C");
		} else {
			Com_Printf(" ");
		}
		Cvar_ValueToStr(var, value, sizeof(value), NULL, 0, NULL, 0);
		Com_Printf (" %s \"%s\"\n", var->name, value);
	}

	Com_Printf ("\n%i total cvars\n", i);
	Com_Printf ("%i cvar indexes\n", cvar_numIndexes);
	Com_Printf("==================================== End Cvar List ====================================\n");
}

/*
============
Cvar_Restart_f

Resets all cvars to their hardcoded values
============
*/
void Cvar_Restart_f( void ) {
	cvar_t	*var;
	cvar_t	**prev;

	prev = &cvar_vars;
	while ( 1 ) {
		var = *prev;
		if ( !var ) {
			break;
		}

		// don't mess with rom values, or some inter-module
		// communication will get broken (com_cl_running, etc)
		if ( var->flags & ( CVAR_ROM | CVAR_INIT | CVAR_NORESTART ) ) {
			prev = &var->next;
			continue;
		}

		// throw out any variables the user created
		if ( var->flags & CVAR_USER_CREATED ) {
			*prev = var->next;

			if(var->type == CVAR_STRING)
			{
				if(var->string != NULL)
				{
					if(var->string != nullstring)
						Z_Free( var->string );
					var->string = NULL;
				}
				if(var->resetString != NULL)
				{
					if(var->resetString != nullstring)
						Z_Free( var->resetString );
					var->resetString = NULL;
				}
				if(var->latchedString != NULL)
				{
					if(var->latchedString != nullstring)
						Z_Free( var->latchedString );
					var->latchedString = NULL;
				}
			}
			if ( var->name ) {
				Z_Free( var->name );
				var->name = NULL;
			}
			if ( var->description ) {
				if(var->description != nullstring)
					Z_Free( var->description );
				var->description = NULL;
			}
			// clear the var completely, since we
			// can't remove the index from the list
			Com_Memset( var, 0, sizeof( cvar_t ) );
			continue;
		}

		Cvar_Reset( var->name );

		prev = &var->next;
	}
}

/*
=====================
Cvar_InfoString
=====================
*/
char	*Cvar_InfoString( int bit ) {
	static char	info[MAX_INFO_STRING];
	char value[1024];

	cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars ; var ; var = var->next) {
		if (var->flags & bit) {
			if(var->type != CVAR_BOOL)
				Cvar_ValueToStr(var, value, sizeof(value), NULL, 0, NULL, 0);
			else
				Com_sprintf(value, sizeof(value), "%d", var->boolean);
			Info_SetValueForKey (info, var->name, value);
		}
	}
	return info;
}

char	*Cvar_InfoString_IW_Wrapper( int dummy, int bit )
{
    return Cvar_InfoString( bit );
}
/*
=====================
Cvar_InfoString_Big

  handles large info strings ( CS_SYSTEMINFO )
=====================
*/
/*
char	*Cvar_InfoString_Big( int bit ) {
	static char	info[BIG_INFO_STRING];
	cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars ; var ; var = var->next) {
		if (var->flags & bit) {
			Info_SetValueForKey_Big (info, var->name, "O_o");
		}
	}
	return info;
}
*/


/*
=====================
Cvar_ForEach
=====================
*/
void Cvar_ForEach( void (*callback)(cvar_t const*, void*), void* handoverarg )
{
	cvar_t	*var;

	for (var = cvar_vars ; var ; var = var->next) {
		callback(var, handoverarg);
	}
}


/*
=====================
Cvar_DisplayableValue
=====================
*/
char	*Cvar_DisplayableValue( cvar_t const *var) {
	static char value[8192];

	if(!var)
		value[0] = '\0';
	else
		Cvar_ValueToStr(var, value, sizeof(value), NULL, 0, NULL, 0);

	return value;
}


/*
=====================
Cvar_GetVariantString
=====================
*/
char	*Cvar_GetVariantString( const char* cvar_name ) {

	cvar_t	const *var = Cvar_FindVar(cvar_name);
	if(var == NULL)
		return "";

	return Cvar_DisplayableValue( var );
}


/*
=====================
Cvar_InfoStringBuffer
=====================
*/
void Cvar_InfoStringBuffer( int bit, char* buff, int buffsize ) {
	Q_strncpyz(buff,Cvar_InfoString(bit),buffsize);
}


/*
==================
Cvar_CompleteCvarName
==================
*/
void Cvar_CompleteCvarName( char *args, int argNum )
{
	if( argNum == 2 )
	{
		// Skip "<cmd> "
		char *p = Com_SkipTokens( args, 1, " " );

		if( p > args )
			Field_CompleteCommand( p, qfalse, qtrue );
	}
}


cvar_t* Cvar_RegisterString(const char* name, const char* string, unsigned short flags, const char* description){

	cvar_t* cvar;
	CvarLimits limits;
	CvarValue value;
	
	limits.imin = 0;
	limits.imax = 0;	
	
	value.string = string;
	
	cvar = Cvar_Register(name, CVAR_STRING, flags, value, limits, description);
	return cvar;
}

cvar_t* Cvar_RegisterBool(const char* name, qboolean boolean, unsigned short flags, const char* description){

	cvar_t* cvar;
	CvarLimits limits;
	CvarValue value;
	
	limits.imin = 0;
	limits.imax = 0;	
	
	value.boolean = boolean;
	
	cvar = Cvar_Register(name, CVAR_BOOL, flags, value, limits, description);
	return cvar;
}

cvar_t* Cvar_RegisterInt(const char* name, int integer, int min, int max, unsigned short flags, const char* description){

	cvar_t* cvar;
	CvarLimits limits;
	CvarValue value;
	
	limits.imin = min;
	limits.imax = max;	
	
	value.integer = integer;
	
	cvar = Cvar_Register(name, CVAR_INT, flags, value, limits, description);
	return cvar;
}


cvar_t* Cvar_RegisterFloat(const char* name, float val, float min, float max, unsigned short flags, const char* description){

	cvar_t* cvar;
	CvarLimits limits;
	CvarValue value;
	
	limits.fmin = min;
	limits.fmax = max;	
	
	value.floatval = val;
	
	cvar = Cvar_Register(name, CVAR_FLOAT, flags, value, limits, description);

	return cvar;

}

cvar_t* Cvar_RegisterVec2(const char* name, float x, float y, float min, float max, unsigned short flags, const char* description){

	cvar_t* cvar;
	CvarLimits limits;
	CvarValue value;
	
	limits.fmin = min;
	limits.fmax = max;
	
	value.vec2[0] = x;
	value.vec2[1] = y;

	cvar = Cvar_Register(name, CVAR_VEC2, flags, value, limits, description);

	return cvar;

}

cvar_t* Cvar_RegisterVec3(const char* name, float x, float y, float z, float min, float max, unsigned short flags, const char* description){

	cvar_t* cvar;
	CvarLimits limits;
	CvarValue value;
	
	limits.fmin = min;
	limits.fmax = max;
	
	value.vec3[0] = x;
	value.vec3[1] = y;
	value.vec3[2] = z;

	cvar = Cvar_Register(name, CVAR_VEC3, flags, value, limits, description);

	return cvar;

}

cvar_t* Cvar_RegisterVec4(const char* name, float x, float y, float z, float imag, float min, float max, unsigned short flags, const char* description){

	cvar_t* cvar;
	CvarLimits limits;
	CvarValue value;
	
	limits.fmin = min;
	limits.fmax = max;
	
	value.vec4[0] = x;
	value.vec4[1] = y;
	value.vec4[2] = z;
	value.vec4[3] = imag;

	cvar = Cvar_Register(name, CVAR_VEC4, flags, value, limits, description);

	return cvar;

}

cvar_t* Cvar_RegisterColor(const char* name, float r, float g, float b, float alpha, unsigned short flags, const char* description){

	cvar_t* cvar;
	CvarLimits limits;
	CvarValue value;
	
	limits.fmin = 0.0;
	limits.fmax = 0.0;
	
	value.color.red = (byte)(0xff * r);
	value.color.green = (byte)(0xff * g);
	value.color.blue = (byte)(0xff * b);
	value.color.alpha = (byte)(0xff * alpha);

	cvar = Cvar_Register(name, CVAR_COLOR, flags, value, limits, description);

	return cvar;

}


cvar_t* Cvar_RegisterEnum(const char* name, const char** strings, int integer, unsigned short flags, const char* description){

	cvar_t* cvar;
	CvarLimits limits;
	CvarValue value;
	
	limits.fmin = 0;
	limits.fmax = 0;
	
	value.enumval.strings = strings;
	value.enumval.integer = integer;

	cvar = Cvar_Register(name, CVAR_ENUM, flags, value, limits, description);

	return cvar;

}


void Cvar_ClearModified(cvar_t* cvar){
	cvar->modified = 0;
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/

void Cvar_Init (void)
{
	Cmd_AddCommand ("toggle", Cvar_Toggle_f);
	Cmd_SetCommandCompletionFunc( "toggle", Cvar_CompleteCvarName );
	Cmd_AddPCommand ("set", Cvar_Set_f, 98);
	Cmd_SetCommandCompletionFunc( "set", Cvar_CompleteCvarName );
	Cmd_AddCommand ("sets", Cvar_SetS_f);
	Cmd_SetCommandCompletionFunc( "sets", Cvar_CompleteCvarName );
	Cmd_AddCommand ("seta", Cvar_SetA_f);
	Cmd_SetCommandCompletionFunc( "seta", Cvar_CompleteCvarName );
	Cmd_AddCommand ("setfromcvar", Cvar_SetFromCvar_f);
	Cmd_SetCommandCompletionFunc( "setfromcvar", Cvar_CompleteCvarName );
	Cmd_AddCommand ("setcvartotime", Cvar_SetToTime_f);
	Cmd_SetCommandCompletionFunc( "setcvartotime", Cvar_CompleteCvarName );
	Cmd_AddCommand ("reset", Cvar_Reset_f);
	Cmd_SetCommandCompletionFunc( "reset", Cvar_CompleteCvarName );
	Cmd_AddCommand ("setu", Cvar_SetU_f);
	Cmd_SetCommandCompletionFunc( "setu", Cvar_CompleteCvarName );
	Cmd_AddPCommand ("cvarlist", Cvar_List_f, 98);
	cvar_cheats = Cvar_RegisterBool("cvar_cheats", qfalse, CVAR_INIT, "Enable cheating");
	cheating_enabled = cvar_cheats->boolean;
}



void Cvar_ChangeResetValue(cvar_t* var, CvarValue value)
{

	int i;

	switch(var->type){
		case CVAR_BOOL:
			if((var->resetBoolean && value.boolean) || (!var->resetBoolean && !value.boolean))
				return;

			if(value.boolean){
				var->resetBoolean = 1;
			}else{
				var->resetBoolean = 0;
			}
			break;
		case CVAR_FLOAT:
			if(var->resetFloatval == value.floatval)
				return;

			if(isnan(value.floatval))
				return;

			if(value.floatval < var->fmin || value.floatval > var->fmax)
			{
				Com_Printf ("\'%g\' is not a valid value for cvar '%s'\n", value.floatval, var->name);
				Com_Printf ("Domain is any float in range between \'%g\' and \'%g\'\n", var->fmin, var->fmax);
				return;
			}
			var->resetFloatval = value.floatval;
			break;
		case CVAR_VEC2:
			for(i = 0; i < 2; i++)
			{
				if(isnan(value.vec2[i]))
					return;
				var->resetVec2[i] = value.vec2[i];
			}
			break;
		case CVAR_VEC3:
			for(i = 0; i < 3; i++)
			{
				if(isnan(value.vec3[i]))
					return;
				var->resetVec3[i] = value.vec3[i];
			}
			break;
		case CVAR_COLOR:
		case CVAR_VEC4:
			for(i = 0; i < 4; i++)
			{
				if(isnan(value.vec4[i]))
					return;
				var->resetVec4[i] = value.vec4[i];
			}
			break;
		case CVAR_ENUM:
			if(var->resetInteger == value.integer)
				return;
			if(var->enumStr == NULL)
				return;
			for(i = 0; var->enumStr[i] != NULL && i != value.integer; i++ );
			if(var->enumStr[i] == NULL)
			{
				Com_Printf ("\'%d\' is not a valid value for cvar '%s'\n", value.integer, var->name);
				Com_Printf ("  Domain is one of the following:\n");
				for(i = 0; var->enumStr[i] != NULL; i++ )
					Com_Printf ("   %d: %s\n", var->enumStr[i]);
				return;
			}
			var->resetInteger = value.integer;
			break;
		case CVAR_INT:
			if(var->resetInteger == value.integer)
				return;

			if(value.integer < var->imin || value.integer > var->imax)
			{
				Com_Printf ("\'%d\' is not a valid value for cvar '%s'\n", value.integer, var->name);
				Com_Printf ("  Domain is any integer in range between \'%d\' and \'%d\'\n", var->imin, var->imax);
				return;
			}
			var->resetInteger = value.integer;
			break;
		case CVAR_STRING:
			if(!value.string)
				return;

			if(var->resetString && !Q_stricmp(var->resetString, value.string))
				return;

			if(var->resetString && var->resetString != nullstring)
				Z_Free(var->resetString);

			var->resetString = CopyString( value.string );
	}

}


void Cvar_AddFlags(cvar_t* var, unsigned short flags)
{
	var->flags |= flags;
}

void Cvar_AddFlagsByName(const char* var_name, unsigned short flags)
{
	cvar_t	*var = Cvar_FindVar(var_name);
	if(!var)
	{
		Com_PrintError("Cvar_AddFlagsByName: Cvar %s does not exist\n", var_name);
	}
	var->flags |= flags;
}


void Cvar_PatchModifiedFlags()
{

	*(int**)0x8123187 = &cvar_modifiedFlags;
	*(int**)0x81231B9 = &cvar_modifiedFlags;
	*(int**)0x8123747 = &cvar_modifiedFlags;
	*(int**)0x8123959 = &cvar_modifiedFlags;
	*(int**)0x81742F2 = &cvar_modifiedFlags;
	*(int**)0x817519D = &cvar_modifiedFlags;
	*(int**)0x81751C3 = &cvar_modifiedFlags;
	*(int**)0x81775B5 = &cvar_modifiedFlags;
	*(int**)0x81775C1 = &cvar_modifiedFlags;
	*(int**)0x81775EF = &cvar_modifiedFlags;
	*(int**)0x817762C = &cvar_modifiedFlags;
	*(int**)0x819E5BC = &cvar_modifiedFlags;
	*(int**)0x819FBAE = &cvar_modifiedFlags;
	*(int**)0x81A0A28 = &cvar_modifiedFlags;
	*(int**)0x8208F2B = &cvar_modifiedFlags;
	*(int**)0x820956C = &cvar_modifiedFlags;
	*(int**)0x81775CA = (int*)(((char*)&cvar_modifiedFlags) +1);

}



