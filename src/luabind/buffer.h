#define BYTEBUFFER_METATABLE "lua_bytebuffer"

#define lua_checkbytebuffer(L,I)	\
	(chk_bytebuffer*)luaL_checkudata(L,I,BYTEBUFFER_METATABLE)

static int32_t lua_bytebuffer_gc(lua_State *L) {
	chk_bytebuffer *b = lua_checkbytebuffer(L,1);
	chk_bytebuffer_finalize(b);
	return 0;
}

typedef struct {
	void (*Push)(chk_luaPushFunctor *self,lua_State *L);
	chk_bytebuffer *data;
}luaBufferPusher;

static void PushBuffer(chk_luaPushFunctor *_,lua_State *L) {
	luaBufferPusher *self = (luaBufferPusher*)_;
	chk_bytebuffer *data = (chk_bytebuffer*)lua_newuserdata(L, sizeof(*data));
	chk_bytebuffer_init(data,self->data->head,self->data->spos,self->data->datasize);
	luaL_getmetatable(L, BYTEBUFFER_METATABLE);
	lua_setmetatable(L, -2);
}

static int32_t lua_new_bytebuffer(lua_State *L) {
	uint32_t size = (uint32_t)luaL_optinteger(L,1,64);
	chk_bytebuffer *b = (chk_bytebuffer*)lua_newuserdata(L, sizeof(*b));
	chk_bytebuffer_init(b,NULL,0,size);
	luaL_getmetatable(L, BYTEBUFFER_METATABLE);
	lua_setmetatable(L, -2);	
	return 1;
}

static int32_t lua_bytebuffer_clone(lua_State *L) {
	chk_bytebuffer *self,*o;
	self = lua_checkbytebuffer(L,1);
	o = (chk_bytebuffer*)lua_newuserdata(L, sizeof(*o));
	chk_bytebuffer_deep_clone(o,self);
	luaL_getmetatable(L, BYTEBUFFER_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static int32_t lua_bytebuffer_readall(lua_State *L) {
	chk_bytebuffer *b = lua_checkbytebuffer(L,1);
	luaL_Buffer     lb;
	char           *in;
	luaL_buffinit(L, &lb);
	in = luaL_buffinitsize(L,&lb,(size_t)b->datasize);
	chk_bytebuffer_read(b,in,b->datasize);
	luaL_pushresultsize(&lb,(size_t)b->datasize);
	return 1;
}

static int32_t lua_bytebuffer_append_string(lua_State *L) {
	const char *str;
	size_t len = 0;
	chk_bytebuffer *b = lua_checkbytebuffer(L,1);
	str = lua_tolstring(L,2,&len);
	do{
		if(str && len > 0) {
			if(0 != chk_bytebuffer_append(b,(uint8_t*)str,(uint32_t)len))
				break;
			return 0;
		}
	}while(0);
	lua_pushstring(L,"append string failed");
	return 1;
}

static void register_buffer(lua_State *L) {

	luaL_Reg bytebuffer_mt[] = {
		{"__gc", lua_bytebuffer_gc},
		{NULL, NULL}
	};

	luaL_Reg bytebuffer_methods[] = {
		{"Clone",    lua_bytebuffer_clone},
		{"Content",  lua_bytebuffer_readall},
		{"AppendStr",lua_bytebuffer_append_string},
		{NULL,     NULL}
	};	

	luaL_newmetatable(L, BYTEBUFFER_METATABLE);
	luaL_setfuncs(L, bytebuffer_mt, 0);

	luaL_newlib(L, bytebuffer_methods);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	lua_newtable(L);
	SET_FUNCTION(L,"New",lua_new_bytebuffer);
}	