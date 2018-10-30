#ifndef CDEFERRED_MANAGER_SERVER_H
#define CDEFERRED_MANAGER_SERVER_H



class CDeferredLight;

class CDeferredManagerServer : public CBaseGameSystem
{
public:

	CDeferredManagerServer();
	~CDeferredManagerServer();

	const char* Name() { return "DeferredManagerServer"; }

	virtual bool Init();
	virtual void Shutdown();

	virtual void LevelInitPreEntity();

	int AddCookieTexture( const char *pszCookie );
	void AddWorldLight( CDeferredLight *l );
};

extern CDeferredManagerServer *GetDeferredManager();

#endif