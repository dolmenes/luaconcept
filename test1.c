#include <assert.h>
#include <stdlib.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

struct Thread {
  struct Thread *prev;
  struct Thread *next;
  lua_State *lua;
};

struct Thread *CurrentThread;
int nThreads = 0;
lua_State *VM;

struct Thread *allocThread( lua_State * );
int iSpawn( lua_State * );

int main( int argc, char **argv ) {
  (void)argc;
  (void)argv;

  int resumeState;
  struct Thread *tmpThread;

  VM = luaL_newstate( );
  if( !VM ) {
    fprintf( stderr, "luaL_newstate( ).\n" );
    exit( EXIT_FAILURE );
  }

  luaL_openlibs( VM );

  lua_pushcfunction( VM, iSpawn );
  lua_setglobal( VM, "spawn" );

  CurrentThread = allocThread( VM );
  if( !CurrentThread ) {
    fprintf( stderr, "Error: memory exhaust !\n" );
    abort( );
  }

  lua_pop( VM, 1 ); // Quitamos el pid del thread recién creado.

  // No hay mas threads. Nos enlazamos con nosotros mismos.
  CurrentThread->prev = CurrentThread;
  CurrentThread->next = CurrentThread;
  nThreads = 1;

  if( luaL_loadfile( CurrentThread->lua, "test1.lua" ) ) {
    fprintf( stderr, "luaL_loadfile( ).\n" );
    exit( EXIT_FAILURE );
  }

  // Bucle mientras haya threads y no se produzcan errores.
  while( nThreads ) {
    resumeState = lua_resume( CurrentThread->lua, 0 );

    switch( resumeState ) {
    case LUA_YIELD: // El thread cedió el control.
      // Si hay mas de 1, cambiamos. En otro caso, seguimos en el mismo.
      if( nThreads > 1 )
        CurrentThread = CurrentThread->next;

      break;

    case 0: // El thread terminó.
      // Si quedan mas, eliminamos el que termino de la lista.
      if( --nThreads ) {
        tmpThread = CurrentThread;

        // Aquí seleccionamos el siguiente thread. Tal y como está,
        // ejecuta primero el que primero se creo. Una FIFO.
        CurrentThread->next->prev = CurrentThread->prev;
        CurrentThread->prev->next = CurrentThread->next;
        CurrentThread = CurrentThread->next;
        free( tmpThread );
      }
      break;

    default: // Error.
      fprintf( stderr, "lua_resume( ).\n" );
      abort( );
    }
  }

  lua_close( VM );
  return EXIT_SUCCESS;
}

struct Thread *allocThread( lua_State *L ) {
  struct Thread *tmp;

  tmp = malloc( sizeof( struct Thread ) );

  if( tmp ) {
    tmp->lua = lua_newthread( L );

    if( !( tmp->lua ) ) {
      free( tmp );
      tmp = NULL;
    }
  }

  return tmp;
}

/*
  spawn( funct, ... ) -> pid.
*/
int iSpawn( lua_State *L ) {
  struct Thread *t;
  int count;

  assert( L == CurrentThread->lua );

  count = lua_gettop( L );

  // Se necesita mínimo un argumento, y ha de ser una función.
  if( ( !count ) || ( !lua_isfunction( L, count ) ) ) {
    fprintf( stderr, "spawn( ): invalid arguments.\n" );
    abort( );
  }

  t = allocThread( L );

  if( !t ) {
    fprintf( stderr, "Error: memory exhaust !\n" );
    abort( );
  }

  // Colocamos el thread en la FIFO de threads pendientes.
  t->next = CurrentThread;
  t->prev = CurrentThread->prev;
  CurrentThread->prev->next = t;
  CurrentThread->prev = t;
  ++nThreads;

  // Movemos el pid, que está en la cima de la pila, justo antes de la
  // función a llamar, para que quede en la cima cuando copiemos los
  // argumentos al thread nuevo.
  lua_insert( L, count );

  // Copiamos todos los argumentos, tal cual, del thread llamante al nuevo.
  lua_xmove( L, t->lua, count );

  return 1; // Dejamos el pid en la pila.
}
