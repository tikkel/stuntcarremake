#ifdef linux
#include "dx_linux.h"
#include "matvec.h"

const char* BitMapRessourceName(const char* name)
{
static const char* resname[] = {"RoadYellowDark", "RoadYellowLight", "RoadRedDark", "RoadRedLight", "RoadBlack", "RoadWhite", 0};
static const char* filename[] = {"Bitmap/RoadYellowDark.bmp", "Bitmap/RoadYellowLight.bmp", "Bitmap/RoadRedDark.bmp", "Bitmap/RoadRedLight.bmp", "Bitmap/RoadBlack.bmp", "Bitmap/RoadWhite.bmp", 0};
	int i = 0;
	while(resname[i] && strcmp(resname[i], name)) i++;
	return filename[i];
}

void IDirect3DTexture9::LoadTexture(const char* name) 
{
	if (texID) glDeleteTextures(1, &texID);
	glGenTextures(1, &texID);
	SDL_Surface *img = IMG_Load(BitMapRessourceName(name));
	if(!img) {
		printf("Warning, image \"%s\" => \"%s\" not loaded\n", name, BitMapRessourceName(name));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		return;
	}
	w = img->w;
	h = img->h;
	w2 = NP2(w);
	h2 = NP2(h);
	wf = (float)w2 / (float)w;
	hf = (float)h2 / (float)h;
	Bind();
	// ugly... Just blindly load the texture without much check!
	glTexParameteri(GL_TEXTURE_2D , GL_TEXTURE_MIN_FILTER , GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D , GL_TEXTURE_MAG_FILTER , GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w2, h2, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, (img->format->BytesPerPixel==3)?GL_BGR:GL_BGRA, GL_UNSIGNED_BYTE, img->pixels);
	UnBind();
	if (img) SDL_FreeSurface(img);
}


struct sound_buffer_t {
	ALuint id;
};

struct sound_source_t {
	ALuint id;
	ALuint buffer;
	bool playing;
};

sound_buffer_t * sound_load(void* data, int size, int bits, int sign, int channels, int freq);
sound_source_t * sound_source( sound_buffer_t * buffer );
void sound_play( sound_source_t * s );
void sound_play_looping( sound_source_t * s );
bool sound_is_playing( sound_source_t * s );
void sound_stop( sound_source_t * s );
void sound_release_source( sound_source_t * s );
void sound_release_buffer( sound_buffer_t * s );
void sound_set_pitch( sound_source_t * s, float freq );
void sound_volume( sound_source_t * s, long decibels );
void sound_pan( sound_source_t * s, long pan );
void sound_position( sound_source_t * s, float x, float y, float z, float min_distance, float max_distance );

void sound_set_position( sound_source_t * s, long newpos );
long sound_get_position( sound_source_t * s );

int npot(int n) {
	int i= 1;
	while(i<n) i<<=1;
	return i;
}

IDirectSoundBuffer8::IDirectSoundBuffer8()
{
	source = NULL;
	buffer = NULL;
}

HRESULT IDirectSoundBuffer8::SetVolume(LONG lVolume)
{
	if (!source)
		return DSERR_GENERIC;
	sound_volume(source, lVolume); 
	return DS_OK;
}

HRESULT IDirectSoundBuffer8::Play(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags) 
{
	if (!source)
		return DSERR_GENERIC;
	if (dwFlags&DSBPLAY_LOOPING) 
		sound_play_looping(source); 
	else 
		sound_play(source); 
	return DS_OK;
}
  
HRESULT IDirectSoundBuffer8::SetFrequency(DWORD dwFrequency)
{
	if (!source)
		return DSERR_GENERIC;
	sound_set_pitch(source, dwFrequency); 
	return DS_OK;
}

HRESULT IDirectSoundBuffer8::SetCurrentPosition(DWORD dwNewPosition)
{
	if (!source)
		return DSERR_GENERIC;
	sound_set_pitch(source, dwNewPosition); 
	return DS_OK;
}

HRESULT IDirectSoundBuffer8::GetCurrentPosition(LPDWORD pdwCurrentPlayCursor, LPDWORD pdwCurrentWriteCursor)
{
	if (!source)
		return DSERR_GENERIC;
	if (pdwCurrentPlayCursor)
		*pdwCurrentPlayCursor = sound_get_position(source);
	return DS_OK;
}

HRESULT IDirectSoundBuffer8::Stop() 
{
	if (!source)
		return DSERR_GENERIC;
	sound_stop(source); 
	return DS_OK;
}

HRESULT IDirectSoundBuffer8::SetPan(LONG lPan)
{
	if (!source)
		return DSERR_GENERIC;
#warning TODO: conversion lPan to OpenAL panning
	sound_pan(source, lPan); 
	return DS_OK;
}

IDirectSoundBuffer8::~IDirectSoundBuffer8()
{
	if (source)
		Release();
}

HRESULT IDirectSoundBuffer8::Release()
{
	if(buffer) {
		sound_release_buffer(buffer);
		buffer = NULL;
	}
	if(source) {
		sound_release_source(source);
		source = NULL;
	}
}

HRESULT IDirectSoundBuffer8::Lock(DWORD dwOffset, DWORD dwBytes, LPVOID * ppvAudioPtr1, LPDWORD  pdwAudioBytes1, LPVOID * ppvAudioPtr2, LPDWORD pdwAudioBytes2, DWORD dwFlags)
{
	if(dwOffset != 0) return E_FAIL;
	*ppvAudioPtr2 = NULL;
	*pdwAudioBytes2 = 0;
	*ppvAudioPtr1 = malloc(dwBytes);
	*pdwAudioBytes1 = dwBytes;
	return S_OK;
}
HRESULT IDirectSoundBuffer8::Unlock(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2)
{
	if(dwAudioBytes2!=0) return E_FAIL;
	if(source || buffer) Release();
	buffer = sound_load(pvAudioPtr1, dwAudioBytes1, 8, 0, 1, 11025);
	source = sound_source(buffer);
	free(pvAudioPtr1);
	return S_OK;
}


HRESULT IDirectSound8::CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, LPDIRECTSOUNDBUFFER * ppDSBuffer, LPUNKNOWN pUnkOuter)
{
	IDirectSoundBuffer8 *tmp = new IDirectSoundBuffer8();
	*ppDSBuffer = tmp;
	return S_OK;
}

HRESULT DirectSoundCreate8(LPCGUID lpcGuidDevice, LPDIRECTSOUND8 * ppDS8, LPUNKNOWN pUnkOuter)
{
	*ppDS8 = new IDirectSound8();
	return DS_OK;
}

/*
 * Matrix
*/
// Try to keep everything column-major to make OpenGL happy...
D3DXMATRIX* D3DXMatrixPerspectiveFovLH(D3DXMATRIX *pOut, FLOAT fovy, FLOAT Aspect, FLOAT zn, FLOAT zf)
{
#if 0
 	float yScale = 1.0f / tanf(fovy/2.0f);
 	float xScale = yScale / Aspect;
	float nf = zn - zf;
	pOut->m[0+ 0] = xScale;
	pOut->m[1+ 4] = yScale;
	pOut->m[2+ 8] = (zf+zn)/nf;
	pOut->m[3+ 8] = -1.0f;
	pOut->m[2+12] = 2*zf*zn/nf;
#else
	const float ymax=zn*tanf(fovy*0.5f);
	const float xmax=ymax*Aspect;
	const float temp=2.0f*zn;
	const float temp2=2.0f*xmax;
	const float temp3=2.0f*ymax;
	const float temp4=zf-zn;
	pOut->m[0]=temp/temp2;
	pOut->m[1]=0.0f;
	pOut->m[2]=0.0f;
	pOut->m[3]=0.0f;
	pOut->m[4]=0.0f;
	pOut->m[5]=temp/temp3;
	pOut->m[6]=0.0f;
	pOut->m[7]=0.0f;
	pOut->m[8]=0.0f;
	pOut->m[9]=0.0f;
	pOut->m[10]=zf/temp4;
	pOut->m[11]=1.0f;
	pOut->m[12]=0.0f;
	pOut->m[13]=0.0f;
	pOut->m[14]=(zn*zf)/(zn-zf);
	pOut->m[15]=0.0f;	
#endif
	return pOut;
}

D3DXMATRIX* D3DXMatrixIdentity(D3DXMATRIX* pOut)
{
	set_identity(pOut->m);
	return pOut;
}

D3DXMATRIX* D3DXMatrixRotationX(D3DXMATRIX* pOut, FLOAT Angle)
{
	matrix_rot(Angle, 1.0f, 0.0f, 0.0f, pOut->m);
	return pOut;
}
D3DXMATRIX* D3DXMatrixRotationY(D3DXMATRIX* pOut, FLOAT Angle)
{
	matrix_rot(Angle, 0.0f, 1.0f, 0.0f, pOut->m);
	return pOut;
}

D3DXMATRIX* D3DXMatrixRotationZ(D3DXMATRIX* pOut, FLOAT Angle)
{
	matrix_rot(Angle, 0.0f, 0.0f, 1.0f, pOut->m);
	return pOut;
}

D3DXMATRIX* D3DXMatrixTranslation(D3DXMATRIX* pOut, FLOAT x, FLOAT y, FLOAT z)
{
	matrix_trans(x, y, z, pOut->m);
	return pOut;
}

D3DXMATRIX* D3DXMatrixMultiply(D3DXMATRIX* pOut, const D3DXMATRIX* pM1, const D3DXMATRIX* pM2)
{
	matrix_mul(pM1->m, pM2->m, pOut->m);
	return pOut;
}

D3DXMATRIX* D3DXMatrixLookAtLH(D3DXMATRIX* pOut, const D3DXVECTOR3* pEye, const D3DXVECTOR3* pAt, const D3DXVECTOR3* pUp)
{
	#if 1
	matrix_lookat(&pEye->x, &pAt->x, &pUp->x, pOut->m);
	#else
	// checked, same as DX9
	glm::vec3 vZ = glm::normalize(at - eye);
	glm::vec3 vX = glm::normalize(glm::cross(up, vZ));
	glm::vec3 vY = glm::cross(vZ, vX);

	*pOut = glm::mat4(	vX.x,			vY.x,			vZ.x,			0.0f,
						vX.y,			vY.y,			vZ.y,			0.0f,
						vX.z,			vY.z,			vZ.z,			0.0f,
						glm::dot(-vX, eye), glm::dot(-vY, eye),	glm::dot(-vZ, eye),	1.0f);
	#endif
	return pOut;
}

uint32_t GetStrideFromFVF(DWORD fvf) {
	uint32_t stride = 0;
	if(fvf & D3DFVF_DIFFUSE) stride += sizeof(DWORD);
	if(fvf & D3DFVF_NORMAL) stride +=3*sizeof(float);
	if(fvf & D3DFVF_XYZ) stride +=3*sizeof(float);
	if(fvf & D3DFVF_XYZRHW) stride += 4*sizeof(float);
	if(fvf & D3DFVF_XYZW) stride += 4*sizeof(float);
	if(fvf & D3DFVF_TEX0) stride += 2*sizeof(float);

	return stride;
}

// IDirect3DDevice9
IDirect3DDevice9::IDirect3DDevice9()
{
	for (int i=0; i<8; i++) {
		colorop[i] = 0;
		colorarg1[i] = 0;
		colorarg2[i] = 0;
		alphaop[i] = 0;
	}
	set_identity(mView.m);
	set_identity(mWorld.m);
	set_identity(mProj.m);
	set_identity(mText.m);
}

IDirect3DDevice9::~IDirect3DDevice9()
{
}

HRESULT IDirect3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State, D3DXMATRIX* pMatrix)
{
	switch (State) 
	{
		case D3DTS_VIEW:
			mView = *pMatrix;
			break;
		case D3DTS_WORLD:
			mWorld = *pMatrix;
			break;
		case D3DTS_PROJECTION:
			mProj = *pMatrix;
			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(mProj.m);
			break;
		case D3DTS_TEXTURE0:
		case D3DTS_TEXTURE1:
		case D3DTS_TEXTURE2:
		case D3DTS_TEXTURE3:
		case D3DTS_TEXTURE4:
			//TODO change active texture...
			mText = *pMatrix;
			glMatrixMode(GL_TEXTURE);
			glLoadMatrixf(mText.m);
			break;
		default:
			printf("Unhandled Matrix SetTransform(%X, %p)\n", State, pMatrix);
	}
	return S_OK;
}

HRESULT IDirect3DDevice9::GetTransform(D3DTRANSFORMSTATETYPE State, D3DXMATRIX* pMatrix)
{
	switch (State) 
	{
		case D3DTS_VIEW:
			*pMatrix = mView;
			break;
		case D3DTS_PROJECTION:
			*pMatrix = mProj;
			break;
		case D3DTS_WORLD:
			*pMatrix = mWorld;
			break;
		case D3DTS_TEXTURE0:
		case D3DTS_TEXTURE1:
		case D3DTS_TEXTURE2:
		case D3DTS_TEXTURE3:
		case D3DTS_TEXTURE4:
			#warning TODO change active texture...
			*pMatrix = mText;
			break;
		default:
			printf("Unhandled Matrix SetTransform(%X, %p)\n", State, pMatrix);
	}
	return S_OK;
}

HRESULT IDirect3DDevice9::SetRenderState(D3DRENDERSTATETYPE State, int Value)
{
	switch (State)
	{
		case D3DRS_ZENABLE:
			if(Value) {
				glDepthMask(GL_TRUE);
				glEnable(GL_DEPTH_TEST);
			} else {
				glDepthMask(GL_FALSE);
				glDisable(GL_DEPTH_TEST);
			}
			break;
		case D3DRS_CULLMODE:
			switch(Value)
			{
				case D3DCULL_NONE:
					glDisable(GL_CULL_FACE);
					break;
				case D3DCULL_CW:
					glFrontFace(GL_CW);
					glCullFace(GL_FRONT);
					glEnable(GL_CULL_FACE);
					break;
				case D3DCULL_CCW:
					glFrontFace(GL_CCW);
					glCullFace(GL_FRONT);
					glEnable(GL_CULL_FACE);
					break;
			}
			break;
		default:
			printf("Unhandled Render State %X=%d\n", State, Value);
	}
	return S_OK;
}

HRESULT IDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
{
	const GLenum primgl[] = {GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN};
	const GLenum prim1[] = {1, 2, 1, 3, 1, 1};
	const GLenum prim2[] = {0, 0, 1, 0, 2, 2};
	if(PrimitiveType<D3DPT_POINTLIST || PrimitiveType>D3DPT_TRIANGLEFAN) {
		printf("Unsupported Primitive %d\n", PrimitiveType);
		return E_FAIL;
	}
	if(PrimitiveCount==0)
		return S_OK;

	GLenum mode = primgl[PrimitiveType-1];

	bool transf = ((fvf & D3DFVF_XYZRHW)==0);
	char* ptr = (char*)buffer[0]->buffer.buffer;
	bool vtx = false, col = false, tex0 = false;
	if(fvf & D3DFVF_XYZ) {
		glVertexPointer(3, GL_FLOAT, stride[0], ptr);
		ptr+=3*sizeof(float);
		vtx = true;
	};
	if(fvf & D3DFVF_XYZW) {
		glVertexPointer(4, GL_FLOAT, stride[0], ptr);
		ptr+=4*sizeof(float);
		vtx = true;
	};
	if(fvf & D3DFVF_XYZRHW) {
		glVertexPointer(2, GL_FLOAT, stride[0], ptr);
		ptr+=4*sizeof(float);
		vtx = true;
	};
	if(fvf & D3DFVF_DIFFUSE) {
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, buffer[0]->buffer.colors);
		ptr+=sizeof(DWORD);
		col = true;
	}
	if(fvf & D3DFVF_TEX0) {
		glTexCoordPointer(2, GL_FLOAT, stride[0], ptr);
		ptr+=2*sizeof(float);
		tex0 = true;
	}

	if (vtx)
		glEnableClientState(GL_VERTEX_ARRAY);
	else
		glDisableClientState(GL_VERTEX_ARRAY);

	if (col)
		glEnableClientState(GL_COLOR_ARRAY);
	else
		glDisableClientState(GL_COLOR_ARRAY);

	if (tex0) {
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		if (colorop[0] <= D3DTOP_DISABLE) {
			glDisable(GL_TEXTURE_2D);
		} else {
			glEnable(GL_TEXTURE_2D);
		}
	} else {
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	if(transf) ActivateWorldMatrix();
	
	glDrawArrays(mode, StartVertex, prim1[PrimitiveType-1]*PrimitiveCount+prim2[PrimitiveType-1]);

	if(transf) DeactivateWorldMatrix();
	return S_OK;
}

void IDirect3DDevice9::ActivateWorldMatrix()
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	float m[16];
	matrix_mul(mView.m, mWorld.m, m);
	glLoadMatrixf(m);
}
void IDirect3DDevice9::DeactivateWorldMatrix()
{
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

HRESULT IDirect3DDevice9::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	if(Stage>7) {
		printf("Unhandled SetTextureStageState(%d, 0x%X, 0x%X)\n", Stage, Type, Value);
		return S_OK;
	}

	glActiveTexture(GL_TEXTURE0+Stage);
	glClientActiveTexture(GL_TEXTURE0+Stage);

	switch(Type)
	{
		case D3DTSS_COLOROP:
			colorop[Stage] = Value;
			switch(Value)
			{
				case D3DTOP_DISABLE:
					glDisable(GL_TEXTURE_2D);
					break;
				case D3DTOP_SELECTARG1:
					if(colorarg1[Stage]==D3DTA_TEXTURE)
						glEnable(GL_TEXTURE_2D);
				case D3DTOP_SELECTARG2:
					if(colorarg2[Stage]==D3DTA_TEXTURE)
						glEnable(GL_TEXTURE_2D);
					break;
				default:
					printf("Unhandled SetTextureStageState(%d, D3DTSS_COLOROP, %d)\n", Stage, Value);
			}
			break;
		case D3DTSS_COLORARG1:
			colorarg1[Stage] = Value;
			if(Value==D3DTA_TEXTURE && colorop[Stage]==D3DTOP_SELECTARG1)
				glEnable(GL_TEXTURE_2D);
			break;
		case D3DTSS_COLORARG2:
			colorarg2[Stage] = Value;
			if(Value==D3DTA_TEXTURE && colorop[Stage]==D3DTOP_SELECTARG2)
				glEnable(GL_TEXTURE_2D);
			break;
		case D3DTSS_ALPHAOP:
			//TODO probably
			break;
		default:
			printf("Unhandled SetTextureStageState(%d, 0x%X, 0x%X)\n", Stage, Type, Value);
	}

	glActiveTexture(GL_TEXTURE0+0);
	glClientActiveTexture(GL_TEXTURE0+0);

	return S_OK;
}

HRESULT IDirect3DDevice9::SetTexture(DWORD Sampler, IDirect3DTexture9 *pTexture)
{
	if(Sampler) {
		glActiveTexture(GL_TEXTURE0+Sampler);
		glClientActiveTexture(GL_TEXTURE0+Sampler);
	}

	pTexture->Bind();

	if(Sampler) {
		glActiveTexture(GL_TEXTURE0);
		glClientActiveTexture(GL_TEXTURE0);
	}
	return S_OK;
}

HRESULT IDirect3DDevice9::Clear(DWORD Count, const D3DRECT *pRects,DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
	GLbitfield clearval = 0;
	if(Flags&D3DCLEAR_STENCIL) {
		glClearStencil(Stencil);
		clearval |= GL_STENCIL_BUFFER_BIT;
	}
	if(Flags&D3DCLEAR_ZBUFFER) {
		glClearDepth(Z);
		clearval |= GL_DEPTH_BUFFER_BIT;
	}
	if(Flags&D3DCLEAR_TARGET) {
		float r,g,b,a;
		r = ((Color>>0 )&0xff)/255.0f;
		g = ((Color>>8 )&0xff)/255.0f;
		b = ((Color>>16)&0xff)/255.0f;
		a = ((Color>>24)&0xff)/255.0f;
		glClearColor(r, g, b, a);
		clearval |= GL_COLOR_BUFFER_BIT;
	}
	if(clearval)
		glClear(clearval);
	return S_OK;
}

HRESULT IDirect3DDevice9::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) {
	*ppVertexBuffer = new IDirect3DVertexBuffer9(Length, FVF);

	return S_OK;
}

HRESULT IDirect3DDevice9::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride) {
	buffer[StreamNumber] = pStreamData;
	offset[StreamNumber] = OffsetInBytes;
	stride[StreamNumber] = Stride;
}

HRESULT IDirect3DDevice9::SetFVF(DWORD FVF) {
	fvf = FVF;
}

IDirect3DVertexBuffer9::IDirect3DVertexBuffer9(uint32_t size, uint32_t fvf) {
	buffer.fvf = fvf;
	buffer.buffer = malloc(size);

	buffer.size = size/GetStrideFromFVF(fvf);
	buffer.colors = (uint32_t*)malloc(buffer.size * 4);
}

IDirect3DVertexBuffer9::~IDirect3DVertexBuffer9() {
	Release();
}

HRESULT IDirect3DVertexBuffer9::Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData,DWORD Flags) {
	// very basic
	*ppbData = (void*)((char*)buffer.buffer + OffsetToLock);
	return S_OK;
}

HRESULT IDirect3DVertexBuffer9::Unlock() {
	// (I told you, very basic)
	if(buffer.fvf&D3DFVF_DIFFUSE) {
		uint32_t stride = GetStrideFromFVF(buffer.fvf);
		char* ptr = (char*)buffer.buffer;
		if(buffer.fvf & D3DFVF_XYZ) {
			ptr+=3*sizeof(float);
		};
		if(buffer.fvf & D3DFVF_XYZW) {
			ptr+=4*sizeof(float);
		};
		if(buffer.fvf & D3DFVF_XYZRHW) {
			ptr+=4*sizeof(float);
		};
		uint32_t* dst = buffer.colors;
		for (int i = 0; i<buffer.size; i++) {
			uint32_t tmp = *(uint32_t*)(ptr);
			tmp = (tmp & 0xff00ff00) | ((tmp & 0xff)<<16) | ((tmp & 0xff0000)>>16);
			*dst = tmp;
			dst++;
			ptr+=stride;
		}
	}
	return S_OK;
}

HRESULT IDirect3DVertexBuffer9::Release() {
	free(buffer.buffer);
	return S_OK;
}


CDXUTTextHelper::CDXUTTextHelper(TTF_Font* font, GLuint sprite, int size) : 
	m_sprite(sprite), m_size(size), m_posx(0), m_posy(0)
{
	// set colors
	m_forecol[0] = m_forecol[1] = m_forecol[2] = m_forecol[3] = 1.0f;
	// setup texture
	m_fontsize = TTF_FontHeight(font);
	glGenTextures(1, &m_texture);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	int w = npot(16*m_fontsize);
	void* tmp = malloc(w*w*4); memset(tmp, 0, w*w*4);
	m_sizew = w; m_sizeh = w;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_sizew, m_sizeh, 0, GL_BGRA, GL_UNSIGNED_BYTE, tmp);
	free(tmp);
	SDL_Color forecol = {255,255,255,255};
	for(int i=0; i<16; i++) {
		for(int j=0; j<16; j++) {
			char text[2] = {(char)(i*16+j), 0};
			SDL_Surface* surf = TTF_RenderText_Blended(font, text, forecol);
			if(surf) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, j*m_fontsize, i*m_fontsize, surf->w, surf->h, GL_BGRA, GL_UNSIGNED_BYTE, surf->pixels);
				SDL_FreeSurface(surf);
			}
		}
	}
	glBindTexture(GL_TEXTURE_2D, 0);
}

CDXUTTextHelper::~CDXUTTextHelper()
{
	glDeleteTextures(1, &m_texture);
}

void CDXUTTextHelper::SetInsertionPos(int x, int y)
{
	m_posx = x;
	m_posy = y;
}

void CDXUTTextHelper::DrawTextLine(const wchar_t* line)
{
	m_w = ((float)m_fontsize)/m_sizew; m_h = ((float)m_fontsize)/m_sizeh;

	// Draw it
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	int oldvp[4];
	glGetIntegerv(GL_VIEWPORT, oldvp);

	glOrtho(0, oldvp[2], oldvp[3], 0, -1, 1); // m_Width and m_Height is the resolution of window

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, m_texture);
	glColor4fv(m_forecol);
	glBegin(GL_QUADS);
	char ch;
	int i=0;
	while((ch=line[i]))
	{
		float col = ch%16, lin = ch/16;
		glTexCoord2f(col*m_w+0,lin*m_h+0); glVertex2f(m_posx+i*m_fontsize, m_posy);
		glTexCoord2f(col*m_w+m_w,lin*m_h+0); glVertex2f(m_posx+i*m_fontsize+m_size-1, m_posy);
		glTexCoord2f(col*m_w+m_w,lin*m_h+m_h); glVertex2f(m_posx+i*m_fontsize+m_fontsize-1, m_posy + m_fontsize-1);
		glTexCoord2f(col*m_w+0,lin*m_h+m_h); glVertex2f(m_posx+i*m_fontsize, m_posy + m_fontsize-1);

		i++;
	}
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	m_posy += m_size;

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);

	//glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

void CDXUTTextHelper::DrawFormattedTextLine(const wchar_t* line, ...)
{
	wchar_t buff[1000];
	va_list args;
  	va_start (args, line);
	vswprintf(buff, 1000, line, args);
	DrawTextLine(buff);
	va_end (args);
}

void CDXUTTextHelper::SetForegroundColor(D3DXCOLOR clr)
{
	m_forecol[0] = clr.r;
	m_forecol[1] = clr.g;
	m_forecol[2] = clr.b;
	m_forecol[3] = clr.a;
}

static IDirect3DDevice9* device = NULL;
IDirect3DDevice9 *DXUTGetD3DDevice()
{
	if (!device)
		device = new IDirect3DDevice9();
	return device;
}

static D3DSURFACE_DESC d3dsurface_desc = {0}; 
const D3DSURFACE_DESC * DXUTGetBackBufferSurfaceDesc()
{
	int vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	d3dsurface_desc.Width = vp[2];
	d3dsurface_desc.Height = vp[3];
	return &d3dsurface_desc;
}

DOUBLE DXUTGetTime()
{
	return ((DOUBLE)SDL_GetTicks())/1000.0;
}

void DXUTReset3DEnvironment()
{
	// NOTHING?
}

#endif
