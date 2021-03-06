#include "org_OpenNI_Samples_Assistant_NativeMethods.h"

#define DEBUG 1

#if DEBUG && defined(ANDROID)
#include <android/log.h>
#  define  LOGD(x...)  __android_log_print(ANDROID_LOG_INFO,"OpenNI.Assistant-JNI",x)
#  define  LOGE(x...)  __android_log_print(ANDROID_LOG_ERROR,"OpenNI.Assistant-JNI",x)
#else
#  define  LOGD(...)  do {} while (0)
#  define  LOGE(...)  do {} while (0)
#endif

#include <XnOpenNI.h>
#include <XnCppWrapper.h>
#include <stdio.h>
#include "color_mapping.h"

using namespace xn;
 
//---------------------------------------------------------------------------
// Marshaling
//---------------------------------------------------------------------------
JNIEXPORT jbyte JNICALL 
Java_org_OpenNI_NativeMethods_readByte(JNIEnv *, jclass, jlong ptr)
{
	return *(jbyte*)ptr;
}

JNIEXPORT jshort JNICALL 
Java_org_OpenNI_NativeMethods_readShort(JNIEnv *, jclass, jlong ptr)
{
	return *(jshort*)ptr;
}

JNIEXPORT jint JNICALL 
Java_org_OpenNI_NativeMethods_readInt(JNIEnv *, jclass, jlong ptr)
{
	return *(jint*)ptr;
}

JNIEXPORT jlong JNICALL 
Java_org_OpenNI_NativeMethods_readLong(JNIEnv *, jclass, jlong ptr)
{
	return *(jlong*)ptr;
}

JNIEXPORT void JNICALL Java_org_OpenNI_NativeMethods_copyToBuffer(JNIEnv *env, jclass, jobject buffer, jlong ptr, jint size)
{
	void* pDest = env->GetDirectBufferAddress(buffer);
	memcpy(pDest, (const void*)ptr, size);
}

//---------------------------------------------------------------------------
// Helpers
//---------------------------------------------------------------------------

void SetOutArgObjectValue(JNIEnv* env, jobject p, jobject value)
{
	jclass cls = env->GetObjectClass(p);
	jfieldID fieldID = env->GetFieldID(cls, "value", "Ljava/lang/Object;");
	env->SetObjectField(p, fieldID, value);
}

void SetOutArgDoubleValue(JNIEnv* env, jobject p, double value)
{
	jclass cls = env->FindClass("java/lang/Double");
	jmethodID ctor = env->GetMethodID(cls, "<init>", "(D)V");
	SetOutArgObjectValue(env, p, env->NewObject(cls, ctor, value));
}

void SetOutArgLongValue(JNIEnv* env, jobject p, XnUInt64 value)
{
	jclass cls = env->FindClass("java/lang/Long");
	jmethodID ctor = env->GetMethodID(cls, "<init>", "(J)V");
	SetOutArgObjectValue(env, p, env->NewObject(cls, ctor, value));
}

void SetOutArgPointerValue(JNIEnv* env, jobject p, const void* value)
{
	SetOutArgLongValue(env, p, (jlong)value);
}

void SetOutArgIntValue(JNIEnv* env, jobject p, int value)
{
	jclass cls = env->FindClass("java/lang/Integer");
	jmethodID ctor = env->GetMethodID(cls, "<init>", "(I)V");
	SetOutArgObjectValue(env, p, env->NewObject(cls, ctor, value));
}

void SetOutArgShortValue(JNIEnv* env, jobject p, short value)
{
	jclass cls = env->FindClass("java/lang/Short");
	jmethodID ctor = env->GetMethodID(cls, "<init>", "(S)V");
	SetOutArgObjectValue(env, p, env->NewObject(cls, ctor, value));
}

void SetOutArgByteValue(JNIEnv* env, jobject p, XnUInt8 value)
{
	jclass cls = env->FindClass("java/lang/Byte");
	jmethodID ctor = env->GetMethodID(cls, "<init>", "(B)V");
	SetOutArgObjectValue(env, p, env->NewObject(cls, ctor, value));
}

void SetOutArgStringValue(JNIEnv* env, jobject p, const XnChar* value)
{
	SetOutArgObjectValue(env, p, env->NewStringUTF(value));
}

//---------------------------------------------------------------------------
// SamplesAssistant 
// Modified by Felipe Herranz(felhr85@gmail.com) in order to get more functionality of BitMapGenerator of Samples Assistant java package
//---------------------------------------------------------------------------
Context *mContext = 0;

DepthGenerator mDepthGen;
UserGenerator mUserGen;
ImageGenerator mImageGen;
IRGenerator mIrGen;

char hasDepthGen = 0;
char hasUserGen = 0;
char hasImageGen = 0;
char hasIrGen = 0;

DepthMetaData depthMD;
ImageMetaData imageMD;
SceneMetaData sceneMD;
IRMetaData irMD;

// local functions
// At least I have to check initGraphics function REMEMBER IT!!!!!
XnStatus initGraphics();
void disposeGraphics();
XnStatus generateBitmapLocal(char useScene, char useDepth, char useImage, char useIr, char useHistogram, int **localPtr);
XnStatus generateBitmapRemote(char useScene, char useDepth, char useImage, char useIr, char useHistogram, int *remoteBuf, int buflen);

/*
 * Class:     org_OpenNI_Samples_Assistant_NativeMethods
 * Method:    initFromContext
 * Signature: (JZZ)I
 */
JNIEXPORT jint JNICALL 
Java_org_OpenNI_Samples_Assistant_NativeMethods_initFromContext
  (JNIEnv *env, jclass cls, jlong pContext, jboolean _hasUserGen, jboolean _hasDepthGen, jboolean _hasImageGen, jboolean _hasIrGen)
{
	LOGD("init_start");
	hasUserGen =  _hasUserGen;
	hasDepthGen = _hasDepthGen;
	hasImageGen = _hasImageGen;
	hasIrGen = _hasIrGen;

	mContext = new Context((XnContext*) pContext);
	
	if (!(hasUserGen || hasDepthGen || hasImageGen || hasIrGen))
	{
		LOGD(" All booleans are false");
		return XN_STATUS_BAD_PARAM;
	}
	int rc;
	if (hasUserGen)
	{
		rc = mContext->FindExistingNode(XN_NODE_TYPE_USER, mUserGen);
		if (rc != XN_STATUS_OK)
		{
			//TODO log&retval
			LOGD("No user node exists!");
			return 1;
		}

		mUserGen.GetUserPixels(0, sceneMD);
	}

	if (hasDepthGen)
	{
		rc = mContext->FindExistingNode(XN_NODE_TYPE_DEPTH, mDepthGen);
		if (rc != XN_STATUS_OK)
		{
			//TODO log&retval
			LOGD("No depth node exists! Check your XML.");
			return 1;
		}
		mDepthGen.GetMetaData(depthMD);
	}

	if(hasImageGen)
	{
		rc = mContext->FindExistingNode(XN_NODE_TYPE_IMAGE,mImageGen);
		if(rc != XN_STATUS_OK)
		{
			LOGD("No image node exists! Check your XML.");
			return 1;
		}
		mImageGen.GetMetaData(imageMD);
	}

	if(hasIrGen)
	{
		rc = mContext->FindExistingNode(XN_NODE_TYPE_IR,mIrGen);
		if(rc != XN_STATUS_OK)
		{
			LOGD("No IR node exists! Check your XML");
			return 1;
		}
		LOGD("Ir Node created");
		mIrGen.GetMetaData(irMD);
	}

	initGraphics();

	LOGD("init_end");
	return XN_STATUS_OK;
}

/*
 * Class:     org_OpenNI_Samples_Assistant_NativeMethods
 * Method:    dispose
 * Signature: ()I
 */
JNIEXPORT jint JNICALL 
Java_org_OpenNI_Samples_Assistant_NativeMethods_dispose (JNIEnv *, jclass)
{
LOGD("dispose_start");

disposeGraphics();

mUserGen.Release();
hasUserGen = 0;
mDepthGen.Release();
hasDepthGen = 0;
mImageGen.Release();
hasUserGen = 0;
mIrGen.Release();
hasIrGen = 0;

mContext->Release();
delete mContext;
mContext = 0;

LOGD("dispose_end");
return XN_STATUS_OK;
}


/*
 * Class:     org_OpenNI_Samples_Assistant_NativeMethods
 * Method:    getMapOutputMode
 * Signature: (Lorg/OpenNI/OutArg;Lorg/OpenNI/OutArg;Lorg/OpenNI/OutArg;)I
 */
JNIEXPORT jint JNICALL 
Java_org_OpenNI_Samples_Assistant_NativeMethods_getMapOutputMode
  (JNIEnv * env, jclass cls, jobject outXRes, jobject outYRes, jobject outFPS)
{
	XnMapOutputMode mode;
	XnNodeHandle hNode = 0;
	
	if(mDepthGen)
	{
		LOGD("Node Depth Handle");
		hNode = (XnNodeHandle)mDepthGen;
	}else if(mUserGen)
	{
		LOGD("Node User Handle");
		hNode = (XnNodeHandle)mUserGen;
	}else if(mImageGen)
	{
		LOGD("Node Image Handle");
		hNode = (XnNodeHandle)mImageGen;
	}else if(mIrGen)
	{
		LOGD("Node IR Handle");
		hNode = (XnNodeHandle)mIrGen;
	}

	XnStatus nRetVal = xnGetMapOutputMode((XnNodeHandle)hNode, &mode);
	XN_IS_STATUS_OK(nRetVal);

	SetOutArgIntValue(env, outXRes, mode.nXRes);
	SetOutArgIntValue(env, outYRes, mode.nYRes);
	SetOutArgIntValue(env, outFPS, mode.nFPS);

	return XN_STATUS_OK;
}

/*
 * Class:     org_OpenNI_Samples_Assistant_NativeMethods
 * Method:    generateBitmapLocalBuffer
 * Signature: (ZZZLorg/OpenNI/OutArg;)I
 */
JNIEXPORT jint JNICALL Java_org_OpenNI_Samples_Assistant_NativeMethods_generateBitmapLocalBuffer
  (JNIEnv *env, jclass cls, jboolean useScene, jboolean useDepth, jboolean useImage, jboolean useIr, jboolean useHistogram, jobject outPtr)
{
	int *bitmap = 0;
	XnStatus nRetVal = generateBitmapLocal(hasUserGen && useScene, hasDepthGen && useDepth, hasImageGen && useImage, hasIrGen && useIr, useHistogram, &bitmap);
	SetOutArgPointerValue(env, outPtr, bitmap);

	XN_IS_STATUS_OK(nRetVal);


	return XN_STATUS_OK;
}

/*
 * Class:     org_OpenNI_Samples_Assistant_NativeMethods
 * Method:    generateBitmapJavaBuffer
 * Signature: (ZZZ[I)I
 */
JNIEXPORT jint JNICALL 
Java_org_OpenNI_Samples_Assistant_NativeMethods_generateBitmapJavaBuffer
  (JNIEnv *env, jclass cls, jboolean useScene, jboolean useDepth, jboolean useImage, jboolean useIr, jboolean useHistogram, jintArray array)
{
	jboolean isCopy;

	jint* buffer = env->GetIntArrayElements(array, &isCopy);
	jsize length = env->GetArrayLength(array);

	
	// do something with the buffer here, replace with something meaningful
	// PAY ATTENTION TO BUFFER OVERFLOW, DO NOT WRITE BEYOND BUFFER LENGTH

	if(isCopy) LOGD("isCopy!");
	XnStatus nRetVal = generateBitmapRemote(hasUserGen && useScene, hasDepthGen && useDepth, hasImageGen && useImage, hasIrGen && useIr, useHistogram, buffer, length);

	// here it is important to use 0 so that JNI takes care of copying
	// the data back to the Java side in case GetByteArrayElements returned a copy
	env->ReleaseIntArrayElements(array, buffer, 0);


	XN_IS_STATUS_OK(nRetVal);
	return XN_STATUS_OK;
}

int *frameBuffer = 0;
size_t BUFSIZE  = sizeof(XnUInt32) * 640 * 480;

/*
 * Class:     org_OpenNI_Samples_Assistant_NativeMethods
 * Method:    readLocalBitmap2JavaBuffer
 * Signature: (J[I)I
 */
JNIEXPORT jint JNICALL Java_org_OpenNI_Samples_Assistant_NativeMethods_readLocalBitmap2JavaBuffer
  (JNIEnv *env, jclass cls, jlong localPtr, jintArray array)
{
//TODO: ignoring localPtr for now. reserved for future use (more than one framebuffer?)
	jboolean isCopy;

	jint* buffer = env->GetIntArrayElements(array, &isCopy);
	jsize length = env->GetArrayLength(array);

	// do something with the buffer here, replace with something meaningful
	// PAY ATTENTION TO BUFFER OVERFLOW, DO NOT WRITE BEYOND BUFFER LENGTH
	if(isCopy) LOGD("isCopy!");
	jsize min = length * sizeof(jint); if (min > BUFSIZE) min = BUFSIZE;
	memcpy(buffer, frameBuffer, min);

	// here it is important to use 0 so that JNI takes care of copying
	// the data back to the Java side in case GetByteArrayElements returned a copy
	env->ReleaseIntArrayElements(array, buffer, 0);

}


//---------------------------------------------------------------------------
// Graphics
//---------------------------------------------------------------------------

size_t MAX_DEPTH = 10000;
//size_t MAX_DEPTH = 4000;
size_t HISTSIZE = MAX_DEPTH * sizeof(float);

float *pHistogram = 0;

XnStatus initGraphics()
{
	frameBuffer = (int *)   malloc(BUFSIZE);
	pHistogram =  (float *) malloc(HISTSIZE);
	if (!frameBuffer || !pHistogram)
		return XN_STATUS_ERROR;
	memset(frameBuffer, 0, BUFSIZE);
	memset(pHistogram,  0, HISTSIZE);

	return XN_STATUS_OK;
}

void disposeGraphics()
{
	free(frameBuffer);
	free(pHistogram);
}

const XnDepthPixel* 	      pDepth;
const XnLabel*			      pLabels;
const XnIRPixel* 			  pIR;
const XnRGB24Pixel*     pRGB;

XnUInt16 nXRes;
XnUInt16 nYRes;
void calcHist()
{
	unsigned int nValue = 0;
	unsigned int nIndex = 0;
	unsigned int nX = 0;
	unsigned int nY = 0;
	unsigned int nNumberOfPoints = 0;

	// Calculate the accumulative histogram
	memset(pHistogram, 0, HISTSIZE);
	for (nY=0; nY<nYRes; nY++)
	{
		for (nX=0; nX<nXRes; nX++)
		{
			nValue = *pDepth;

			if (nValue != 0)
			{
				pHistogram[nValue]++;
				nNumberOfPoints++;
			}

			pDepth++;
		}
	}

	for (nIndex=1; nIndex<MAX_DEPTH; nIndex++)
	{
		pHistogram[nIndex] += pHistogram[nIndex-1];
	}
	if (nNumberOfPoints)
	{
		for (nIndex=1; nIndex<MAX_DEPTH; nIndex++)
		{
			pHistogram[nIndex] = (unsigned int)(256 * (1.0f - (pHistogram[nIndex] / nNumberOfPoints)));
		}
	}
}


XnFloat Colors[][3] =
{
//	{R,G,B}
	{0,1,0},
	{1,0,0},
	{1,1,0},
	{0,1,1},
	{0,0,1},
	{1,.5,0},
	{.5,1,0},
	{0,.5,1},
	{.5,0,1},
	{1,1,.5},
	{1,1,1}
};
XnUInt32 nColors = 10;

void fillBitmap(int *dstBuf, char useScene, char useDepth, char useImage, char useIr, char useHistogram, char drawBackground)
{
	unsigned int nValue = 0;
	float nHistValue = 0;
	unsigned int nIndex = 0;
	XnUInt32 nColorID;
	// RGB pixels
	XnUInt8 rpx;
	XnUInt8 gpx;
	XnUInt8 bpx;

	unsigned char *pDestImage = (unsigned char *)dstBuf;
	// Prepare the texture map
	for (nIndex=0; nIndex < nXRes * nYRes; ++nIndex)
	{
		pDestImage[0] = 0;   //B
		pDestImage[1] = 0;   //G
		pDestImage[2] = 0;   //R
		pDestImage[3] = 255; //A

		if (drawBackground || !useScene || (useScene && *pLabels != 0) || useImage || useIr) // Not sure about this.
		{
			
			if(useDepth)
			{
				nValue = *pDepth;
			}else if(useIr)
			{
				nValue = *pIR;
			}
			else
			{
				nValue = 4000;
			}

			if(useScene)
			{
				XnLabel label = *pLabels;
				if (label == 0) {
					nColorID = nColors;
				} else {
					nColorID = ((label-1) % nColors);
				}
			} else {
				nColorID = nColors;
			}

			if (nValue != 0)
			{
				if(useDepth)
				{
					if(useHistogram)
					{
						nHistValue = pHistogram[nValue];
					}
					else
					{
						//nHistValue = nValue * 255.0 / 4000;
						//nHistValue = normalized_mapping(&nValue);
						//nHistValue = two_section_mapping(&nValue,1500,5);
						//nHistValue = log_mapping_base10(&nValue);
						nHistValue = simple_mapping(&nValue);
					}
					pDestImage[2] = nHistValue * Colors[nColorID][0]; 
					pDestImage[1] = nHistValue * Colors[nColorID][1];
					pDestImage[0] = nHistValue * Colors[nColorID][2];
				}else if(useImage)
				{
					if(useHistogram)
					{
						// Histogram for RGB not developed yet
						nHistValue = pHistogram[nValue];
					}
					else
					{
						rpx = pRGB->nRed;
						gpx = pRGB->nGreen;
						bpx = pRGB->nBlue;
					}
					pDestImage[2] = rpx;
					pDestImage[1] = gpx;
					pDestImage[0] = bpx;
				}else if(useIr)
				{
					if(useHistogram)
					{
						// Histogram for IR not developed yet
						nHistValue = pHistogram[nValue];
					}else
					{
						//LOGD("nValue %d",nValue);
						//nHistValue = nValue * 255.0 / 65535;
						nHistValue = nValue * 255.0 / 1022;
					}
					pDestImage[2] = nHistValue * Colors[nColorID][0]; 
				        pDestImage[1] = nHistValue * Colors[nColorID][1];
				        pDestImage[0] = nHistValue * Colors[nColorID][2];
				}else {
					nHistValue = 255;
				}

				//pDestImage[2] = nHistValue * Colors[nColorID][0]; 
				//pDestImage[1] = nHistValue * Colors[nColorID][1];
				//pDestImage[0] = nHistValue * Colors[nColorID][2];
			}
		}

		pDepth++;
		pLabels++;
		pRGB++;
		pIR++;
		pDestImage += 4;
	}
}

XnStatus prepare(char useScene, char useDepth, char useImage, char useIr, char useHistogram)
{
//TODO handle possible failures! Gotcha!
	if (useDepth)
	{
		mDepthGen.GetMetaData(depthMD);
		nXRes = depthMD.XRes();
		nYRes = depthMD.YRes();

		pDepth = depthMD.Data();

		if (useHistogram)
		{
			calcHist();

			// rewind the pointer
			pDepth = depthMD.Data();
		}
	}
	if (useScene) 
	{
		mUserGen.GetUserPixels(0, sceneMD);
		nXRes = sceneMD.XRes();
		nYRes = sceneMD.YRes();

		pLabels = sceneMD.Data();
	}
	if (useImage)
	{
		mImageGen.GetMetaData(imageMD);
		nXRes = imageMD.XRes();
		nYRes = imageMD.YRes();

		pRGB = imageMD.RGB24Data();
		// HISTOGRAM?????
	}
	if (useIr)
	{
		mIrGen.GetMetaData(irMD);
		nXRes = irMD.XRes();
		nYRes = irMD.YRes();

		pIR = irMD.Data();
		// HISTOGRAM????
	}
}

XnStatus generateBitmapLocal(char useScene, char useDepth, char useImage, char useIr,  char useHistogram, int **localPtr)
{
	*localPtr = 0;
	XnStatus rc = prepare(useScene, useDepth, useImage, useIr, useHistogram);
	//TODO handle errors

	fillBitmap(frameBuffer, useScene, useDepth, useImage, useIr, useHistogram, 1);

	*localPtr = frameBuffer;
	return XN_STATUS_OK;
}

XnStatus generateBitmapRemote(char useScene, char useDepth, char useImage, char useIr, char useHistogram, int *remoteBuf, int buflen)
{
	XnStatus rc = prepare(useScene, useDepth, useImage, useIr, useHistogram);
	//TODO handle errors

	if (buflen < nXRes * nYRes)
	{
		return XN_STATUS_INVALID_BUFFER_SIZE;
	}

	fillBitmap(remoteBuf, useScene, useDepth, useImage, useIr, useHistogram, 1);

	return XN_STATUS_OK;
}

