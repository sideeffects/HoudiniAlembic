#include "SOP_AlembicIn.h"

#include <Alembic/AbcCoreHDF5/All.h>

#include <OP/OP_OperatorTable.h>
#include <OP/OP_Director.h>
#include <PRM/PRM_Include.h>

#include <UT/UT_Interrupt.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimNURBSurf.h>
#include <GU/GU_PrimNURBCurve.h>
#include <GU/GU_PrimPart.h>
#include <GU/GU_PrimPoly.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_DMatrix4.h>

#include <boost/tokenizer.hpp>
#include <sstream>

//-*****************************************************************************

OP_Node * SOP_AlembicIn::myConstructor( OP_Network *net, const char *name,
        OP_Operator *op)
{
    return new SOP_AlembicIn( net, name, op );
}

//-*****************************************************************************

static PRM_Name prm_fileNameName("fileName", "File Name");
static PRM_Name prm_frameName("frame", "Frame");
static PRM_Name prm_fpsName("fps", "Frames Per Second");
static PRM_Name prm_objectPathName("objectPath", "Object Path");
static PRM_Name prm_includeXformName("includeXform", "Include Xform");

static PRM_Default prm_frameDefault(1, "$F");
static PRM_Default prm_fpsDefault(24, "$FPS");
static PRM_Default prm_includeXformDefault(true);

PRM_Template SOP_AlembicIn::myTemplateList[] =
{
    PRM_Template(PRM_FILE, 1, &prm_fileNameName),
    PRM_Template(PRM_FLT_J, 1, &prm_frameName, &prm_frameDefault),
    PRM_Template(PRM_FLT_J, 1, &prm_fpsName, &prm_fpsDefault),
    PRM_Template(PRM_STRING, 1, &prm_objectPathName),
    PRM_Template(PRM_TOGGLE, 1, &prm_includeXformName, &prm_includeXformDefault),
    PRM_Template()
};

//-*****************************************************************************

SOP_AlembicIn::SOP_AlembicIn(OP_Network *net, const char *name,
        OP_Operator *op)
: SOP_Node(net, name, op)
{
}

//-*****************************************************************************

SOP_AlembicIn::~SOP_AlembicIn()
{
}

//-*****************************************************************************

bool SOP_AlembicIn::unloadData()
{
    bool unloaded = SOP_Node::unloadData();
    
    if (unloaded)
    {
    }
    
    return unloaded;
}

//-*****************************************************************************

OP_ERROR SOP_AlembicIn::cookMySop(OP_Context &context)
{
    const float now = context.myTime;
    
    std::string fileName;
    {
        UT_String str;
        evalString(str, "fileName", 0, now);
        
        fileName = str.toStdString();
    }
    
    PathList pathList;
    
    std::string objectPath;
    {
        UT_String str;
        evalString(str, "objectPath", 0, now);
        
        objectPath = str.toStdString();
    }
    
    if ( !objectPath.empty() )
    {
        typedef boost::char_separator<char> Separator;
        typedef boost::tokenizer<Separator> Tokenizer;
        
        Tokenizer tokenizer( objectPath, Separator( "/" ) );
        
        for ( Tokenizer::iterator iter = tokenizer.begin() ;
                iter != tokenizer.end() ; ++iter )
        {
            if ( (*iter).empty() ) { continue; }
            pathList.push_back( *iter );
        }
    }
    
    M44d xform;
    
    gdp->clearAndDestroy();
    
    Args args;
    
    double fps = evalFloat("fps", 0, now);
    args.abcTime = evalFloat("frame", 0, now) / fps;
    args.includeXform = evalInt("includeXform", 0, now);
    
    try
    {
        IArchive archive(::Alembic::AbcCoreHDF5::ReadArchive(),
                fileName);//, ReadArraySampleCachePtr()); //no library-level caching
        
        attachDetailStringData("abcFileName", fileName);
        
        IObject root = archive.getTop();
        
        if ( pathList.empty() ) //walk the entire scene
        {
            for ( size_t i = 0; i < root.getNumChildren(); ++i )
            {
                walkObject( args, root, root.getChildHeader(i),
                            pathList.end(), pathList.end(), xform);
            }
        }
        else //walk to a location + its children
        {
            PathList::const_iterator I = pathList.begin();
            
            const ObjectHeader *nextChildHeader =
                    root.getChildHeader( *I );
            if ( nextChildHeader != NULL )
            {
                walkObject( args, root, *nextChildHeader, I+1, pathList.end(),
                        xform);
            }
        }
    }
    catch ( const std::exception &e )
    {
        std::ostringstream buffer;
        buffer << "Alembic exception: ";
        buffer << e.what();
        addWarning(SOP_MESSAGE, buffer.str().c_str());
    }
    
    return error();
}

//-*****************************************************************************

void SOP_AlembicIn::nodeUnlocked()
{
    //TODO
}

//-*****************************************************************************

GA_ROAttributeRef  SOP_AlembicIn::attachDetailStringData(
        const std::string &attrName, const std::string &value)
{
    GA_RWAttributeRef attrIdx = gdp->findStringTuple(GA_ATTRIB_GLOBAL,
	    attrName.c_str());
    if (attrIdx.isInvalid())
    {
        attrIdx = gdp->addStringTuple(GA_ATTRIB_GLOBAL, attrName.c_str(), 1);

        if (error() >= UT_ERROR_ABORT || attrIdx.isInvalid())
        {
            std::ostringstream buffer;
            buffer << "Could not create detail string attribute: ";
            buffer << attrName;
            addWarning(SOP_MESSAGE, buffer.str().c_str());
        }
    }
    const GA_AIFStringTuple	*aifstring = attrIdx.getAIFStringTuple();
    if (aifstring)
    {
	aifstring->setString(attrIdx.getAttribute(), GA_Offset(0),
			value.c_str(), 0);
    }
    else
    {
       return GA_ROAttributeRef();
    }
    
    return attrIdx;
}

//-*****************************************************************************

void SOP_AlembicIn::walkObject( Args & args, IObject parent, const ObjectHeader &ohead,
            PathList::const_iterator I, PathList::const_iterator E,
                    M44d parentXform)
{
    //for now, let's be time-dependent
    //OP_Node::flags().setTimeDep(true);
    
    //set this if we should continue traversing
    IObject nextParentObject;
    
    if ( IXform::matches( ohead ) )
    {
        if (args.includeXform)
        {
            
            IXform xform( parent, ohead.getName() );
            IXformSchema &xs = xform.getSchema();
            
            XformSample xformSample = xs.getValue(
                    ISampleSelector( args.abcTime ));
            
            M44d m = xformSample.getMatrix();
            
            if (xformSample.getInheritsXforms())
            {
                parentXform = m * parentXform;
            }
            else
            {
                parentXform = m;
            }
            
            nextParentObject = xform;
        }
        else
        {
            //if we're not processing transforms, just grab the child object
            //and move on
            nextParentObject = IObject(parent, ohead.getName());
        }
    }
    else if ( ISubD::matches( ohead ) )
    {
        ISubD subd( parent, ohead.getName() );
        buildSubD( args, subd, parentXform );
        
        nextParentObject = subd;
    }
    else if ( IPolyMesh::matches( ohead ) )
    {
        IPolyMesh polymesh( parent, ohead.getName() );
        
        buildPolyMesh( args, polymesh, parentXform );
        
        nextParentObject = polymesh;
    }
    else
    {
//         std::cerr << "could not determine type of " << ohead.getName()
//                   << std::endl; 
//         std::cerr << ohead.getName() << " has MetaData: "
//                   << ohead.getMetaData().serialize() << std::endl;
    }
    
    
    
    if ( nextParentObject.valid() )
    {
        if ( I == E )
        {
            for ( size_t i = 0; i < nextParentObject.getNumChildren(); ++i )
            {
                walkObject( args, nextParentObject,
                        nextParentObject.getChildHeader( i ), I, E,
                                parentXform);
            }
        }
        else
        {
            const ObjectHeader *nextChildHeader =
                nextParentObject.getChildHeader( *I );
            
            if ( nextChildHeader != NULL )
            {
                walkObject( args, nextParentObject,
                        *nextChildHeader, I+1, E,
                                parentXform);
            }
        }
    }
}

//-*****************************************************************************

std::string SOP_AlembicIn::getFullName( IObject object )
{
    std::string result = object.getFullName();
    
    if (result.size() > 4 && result.substr(0, 5) == "/ABC/")
    {
        return result.substr( 4, result.size() - 1 );
    }
    
    return result;
}

//-*****************************************************************************

void SOP_AlembicIn::addUVs(Args & args, IV2fGeomParam param,
        size_t startPointIdx, size_t startPrimIdx)
{
    if (!param.valid()) { return; }
    
    
    GA_RWAttributeRef uvAttrIndex;
    switch (param.getScope())
    {
    case kVaryingScope:
    case kVertexScope:
    {
        addOrFindTextureAttribute(GEO_POINT_DICT, uvAttrIndex);
        break;
    }
    case kFacevaryingScope:
    {
        addOrFindTextureAttribute(GEO_VERTEX_DICT, uvAttrIndex);
        break;
    }
    default:
        break;
    }
    
    if (uvAttrIndex.isValid())
    {
        processArbitraryGeomParam<IV2fGeomParam, float>(
                    args,
                    param.getParent(),
                    param.getHeader(),
                    GA_STORE_REAL32,
		    GA_TYPE_VOID,
                    uvAttrIndex,
                    startPointIdx,
                    startPrimIdx);
    }
}

//-*****************************************************************************

void SOP_AlembicIn::addNormals(Args & args, IN3fGeomParam param,
        size_t startPointIdx, size_t startPrimIdx)
{
    if (!param.valid()) { return; }
    
    
    GA_RWAttributeRef nAttrIndex;
    switch (param.getScope())
    {
    case kVaryingScope:
    case kVertexScope:
    {
        addOrFindNormalAttribute(GEO_POINT_DICT, nAttrIndex);
        break;
    }
    case kFacevaryingScope:
    {
        addOrFindNormalAttribute(GEO_VERTEX_DICT, nAttrIndex);
        break;
    }
    default:
        break;
    }
    
    if (nAttrIndex.isValid())
    {
        processArbitraryGeomParam<IN3fGeomParam, float>(
                    args,
                    param.getParent(),
                    param.getHeader(),
                    GA_STORE_REAL32,
		    GA_TYPE_NORMAL,
                    nAttrIndex,
                    startPointIdx,
                    startPrimIdx);
    }
    
}

//-*****************************************************************************

bool SOP_AlembicIn::addOrFindTextureAttribute(GEO_AttributeOwner owner,
        GA_RWAttributeRef & attrIdx)
{
    attrIdx = gdp->findTextureAttribute(owner);
    
    if (!attrIdx.isValid())
    {
        attrIdx = gdp->addTextureAttribute(owner);
        
        if (error() >= UT_ERROR_ABORT || !attrIdx.isValid())
        {
            addError(SOP_MESSAGE, "could not create texture attribute.");
            return false;
        }
    }
    return true;
}

//-*****************************************************************************

bool SOP_AlembicIn::addOrFindNormalAttribute(GEO_AttributeOwner owner,
        GA_RWAttributeRef & attrIdx)
{
    attrIdx = gdp->findNormalAttribute(owner);
    
    if (!attrIdx.isValid())
    {
        attrIdx = gdp->addNormalAttribute(owner);
 
        if (error() >= UT_ERROR_ABORT || !attrIdx.isValid())
        {
            addError(SOP_MESSAGE, "could not create Normal attribute.");
            return false;
        }
    }
    return true;
}

//-*****************************************************************************

void SOP_AlembicIn::addArbitraryGeomParams(Args & args,
        ICompoundProperty parent,
        size_t startPointIdx,
        size_t startPrimIdx)
{
    if (!parent.valid()) return;
    
    for (size_t i = 0; i < parent.getNumProperties(); ++i)
    {
        const PropertyHeader &propHeader = parent.getPropertyHeader(i);
        //const std::string &propName = propHeader.getName();
        
        if (IFloatGeomParam::matches(propHeader))
        {
            processArbitraryGeomParam<IFloatGeomParam, float>(
                    args, parent, propHeader,
                    GA_STORE_REAL32,
		    GA_TYPE_VOID,
                    GA_RWAttributeRef(), startPointIdx, startPrimIdx);
        }
        else if (IInt32GeomParam::matches(propHeader))
        {
            processArbitraryGeomParam<IInt32GeomParam, int>(
                    args, parent, propHeader,
                    GA_STORE_INT32,
		    GA_TYPE_VOID,
                    GA_RWAttributeRef(), startPointIdx, startPrimIdx);
        }
        else if (IStringGeomParam::matches(propHeader))
        {
            processArbitraryGeomParam<IStringGeomParam, std::string>(
                    args, parent, propHeader,
                    GA_STORE_STRING,
		    GA_TYPE_VOID,
                    GA_RWAttributeRef(), startPointIdx, startPrimIdx);
                    
        }
        else if (IV2fGeomParam::matches(propHeader))
        {
            processArbitraryGeomParam<IV2fGeomParam, float>(
                    args, parent, propHeader,
                    GA_STORE_REAL32,
		    GA_TYPE_VOID,
                    GA_RWAttributeRef(), startPointIdx, startPrimIdx);
        }
        else if (IV3fGeomParam::matches(propHeader))
        {
            processArbitraryGeomParam<IV3fGeomParam, float>(
                    args, parent, propHeader,
                    GA_STORE_REAL32,
		    GA_TYPE_VECTOR,
                    GA_RWAttributeRef(), startPointIdx, startPrimIdx);
        }
        else if (IN3fGeomParam::matches(propHeader))
        {
            processArbitraryGeomParam<IN3fGeomParam, float>(
                    args, parent, propHeader,
                    GA_STORE_REAL32,
		    GA_TYPE_NORMAL,
                    GA_RWAttributeRef(), startPointIdx, startPrimIdx);
            
        }
        else if (IC3fGeomParam::matches(propHeader))
        {
            processArbitraryGeomParam<IC3fGeomParam, float>(
                    args, parent, propHeader,
                    GA_STORE_REAL32,
		    GA_TYPE_VOID,
                    GA_RWAttributeRef(), startPointIdx, startPrimIdx);
        }
        
    }
}

//-*****************************************************************************

template <typename geomParamT, typename podT>
void SOP_AlembicIn::processArbitraryGeomParam(
        Args & args,
        ICompoundProperty parent,
        const PropertyHeader & propHeader,
        GA_Storage attrStorage,
	GA_TypeInfo attrTypeInfo,
        const GA_RWAttributeRef & existingAttr,
        size_t startPointIdx,
        size_t startPrimIdx
)
{
    GA_RWAttributeRef attrIdx = existingAttr;
    
    geomParamT param(parent, propHeader.getName());
    
    ISampleSelector sampleSelector( args.abcTime );
    typename geomParamT::sample_type paramSample;
    param.getExpanded(paramSample, sampleSelector);
    
    size_t extent = geomParamT::prop_type::traits_type::dataType().getExtent();
    size_t arrayExtent = param.getArrayExtent();
    size_t totalExtent = extent*arrayExtent;
    
    GA_AttributeOwner	owner = GA_ATTRIB_POINT;
    switch (paramSample.getScope())
    {
	case kVaryingScope:
	case kVertexScope:
	    // TODO: For NURBS, kVertexScope should use GA_ATTRIB_VERTEX
	    owner = GA_ATTRIB_POINT;
	    break;
	case kFacevaryingScope:
	    owner = GA_ATTRIB_VERTEX;
	    break;
        case kUniformScope:
        case kConstantScope:
	    owner = GA_ATTRIB_PRIMITIVE;
    }
    if (!attrIdx.isValid())
    {
	if (GAisIntStorage(attrStorage))
	{
	    attrIdx = gdp->findIntTuple(owner, propHeader.getName().c_str(),
			    totalExtent);
	    if (!attrIdx.isValid())
	    {
		attrIdx = gdp->addIntTuple(owner,
			propHeader.getName().c_str(), totalExtent,
			GA_Defaults(), NULL, NULL, attrStorage);
	    }
	}
	else if (GAisFloatStorage(attrStorage))
	{
	    attrIdx = gdp->findFloatTuple(owner, propHeader.getName().c_str(),
			    totalExtent);
	    if (!attrIdx.isValid())
	    {
		attrIdx = gdp->addFloatTuple(owner,
			propHeader.getName().c_str(), totalExtent,
			GA_Defaults(), NULL, NULL, attrStorage);
	    }
	}
	else
	{
	    UT_ASSERT(attrStorage == GA_STORE_STRING);
	    attrIdx = gdp->findStringTuple(owner, propHeader.getName().c_str(),
			    totalExtent);
	    if (!attrIdx.isValid())
	    {
		attrIdx = gdp->addStringTuple(owner,
			propHeader.getName().c_str(), totalExtent);
	    }
	}
    }
    
    if (!attrIdx.isValid())
    {
        //TODO, indicate as error, for now just skip it
        return;
    }
    
    applyArbitraryGeomParamSample<typename geomParamT::sample_type, podT>(
            paramSample,
            attrIdx,
            totalExtent,
            startPointIdx,
            startPrimIdx);
}

//-*****************************************************************************

template <typename geomParamSampleT, typename podT>
void SOP_AlembicIn::applyArbitraryGeomParamSample(
        geomParamSampleT & paramSample,
        const GA_RWAttributeRef & attrIdx,
        size_t totalExtent,
        size_t startPointIdx,
        size_t startPrimIdx
)
{
    switch (paramSample.getScope())
    {
    case kUniformScope:
    {
        const podT * values =reinterpret_cast<const podT *>(
                paramSample.getVals()->get());
        
        size_t i = 0;
        for (size_t primIdx = startPrimIdx, endPrimIdx = gdp->primitives().entries();
                primIdx < endPrimIdx; ++primIdx, ++i)
        {
            GEO_Primitive *prim = gdp->primitives()(primIdx);
            prim->set(attrIdx, values+i*totalExtent, totalExtent);
        }
        
        break;
    }
    case kVaryingScope:
    case kVertexScope:
    {
        const podT * values = reinterpret_cast<const podT *>(
                paramSample.getVals()->get());
        
        size_t i = 0;
        for (size_t pointIdx = startPointIdx, endPointIdx = gdp->points().entries();
                pointIdx < endPointIdx; ++pointIdx, ++i)
        {
            GEO_Point *point = gdp->points()(pointIdx);
            point->set(attrIdx, values+i*totalExtent, totalExtent);
        }
        
        break;
    }
    case kFacevaryingScope:
    {
        const podT * values = reinterpret_cast<const podT *>(
                    paramSample.getVals()->get());
        
        size_t vertexIdx = 0;
        for (size_t primIdx = startPrimIdx, endPrimIdx = gdp->primitives().entries();
                primIdx < endPrimIdx; ++primIdx)
        {
            GEO_Primitive *prim = gdp->primitives()(primIdx);
            
            
            int vtxCount = prim->getVertexCount();
            for(int i=0; i < vtxCount; ++i, ++vertexIdx)
            {
                GEO_Vertex *vtx = &prim->getVertex(i);
                vtx->set(attrIdx, values+vertexIdx*totalExtent, totalExtent);
            }
        }
        
        break;
    }
    
    case kConstantScope:
    default:
    {
        const podT * values = reinterpret_cast<const podT *>(
                paramSample.getVals()->get());
        for (size_t primIdx = startPrimIdx, endPrimIdx = gdp->primitives().entries();
                primIdx < endPrimIdx; ++primIdx)
        {
            GEO_Primitive *prim = gdp->primitives()(primIdx);
            prim->set(attrIdx, values, totalExtent);
        }
        
        break;
    }
    }
}


//-*****************************************************************************
//specialized for handling the string case
template <>
void SOP_AlembicIn::applyArbitraryGeomParamSample<
            IStringGeomParam::sample_type, std::string>(
        IStringGeomParam::sample_type & paramSample,
        const GA_RWAttributeRef & attrIdx,
        size_t totalExtent,
        size_t startPointIdx,
        size_t startPrimIdx
)
{//TACO
    switch (paramSample.getScope())
    {
    case kUniformScope:
    {
        const std::string * values =reinterpret_cast<const std::string *>(
                paramSample.getVals()->get());
        
        size_t i = 0;
        for (size_t primIdx = startPrimIdx, endPrimIdx = gdp->primitives().entries();
                primIdx < endPrimIdx; ++primIdx, ++i)
        {
            GEO_Primitive *prim = gdp->primitives()(primIdx);
            
            for (size_t j = 0; j < totalExtent; ++j)
            {
                prim->setString(attrIdx, values[i*totalExtent+j].c_str(), j);
            }
        }
        
        break;
    }
    case kVaryingScope:
    case kVertexScope:
    {
        const std::string * values = reinterpret_cast<const std::string *>(
                paramSample.getVals()->get());
        
        size_t i = 0;
        for (size_t pointIdx = startPointIdx, endPointIdx = gdp->points().entries();
                pointIdx < endPointIdx; ++pointIdx, ++i)
        {
            GEO_Point *point = gdp->points()(pointIdx);
            //point->set(attrIdx, values+i*totalExtent, totalExtent);
            
            for (size_t j = 0; j < totalExtent; ++j)
            {
                point->setString(attrIdx, values[i*totalExtent+j].c_str(), j);
            }
        }
        
        break;
    }
    case kFacevaryingScope:
    {
        const std::string * values = reinterpret_cast<const std::string *>(
                    paramSample.getVals()->get());
        
        size_t vertexIdx = 0;
        for (size_t primIdx = startPrimIdx, endPrimIdx = gdp->primitives().entries();
                primIdx < endPrimIdx; ++primIdx)
        {
            GEO_Primitive *prim = gdp->primitives()(primIdx);
            
            
            int vtxCount = prim->getVertexCount();
            for(int i=0; i < vtxCount; ++i, ++vertexIdx)
            {
                GEO_Vertex vtx = prim->getVertex(i);

                for (size_t j = 0; j < totalExtent; ++j)
                {
		    vtx.setString(attrIdx,
			    values[vertexIdx*totalExtent+j].c_str(), j);
                }
            
            }
        }
        
        break;
    }
    
    case kConstantScope:
    default:
    {
        const std::string * values = reinterpret_cast<const std::string *>(
                paramSample.getVals()->get());
        
        for (size_t primIdx = startPrimIdx, endPrimIdx = gdp->primitives().entries();
                primIdx < endPrimIdx; ++primIdx)
        {
            GEO_Primitive *prim = gdp->primitives()(primIdx);

            for (size_t j = 0; j < totalExtent; ++j)
            {
                prim->setString(attrIdx, values[j].c_str(), j);
            }
            
        }
        
        break;
    }
    }
}

//-*****************************************************************************

void SOP_AlembicIn::buildSubD( Args & args, ISubD &subd, M44d parentXform )
{
    ISampleSelector sampleSelector( args.abcTime );
    ISubDSchema &ss = subd.getSchema();
    ISubDSchema::Sample sample = ss.getValue( sampleSelector );
    
    
    //store the primitive and point start indices
    size_t startPointIdx = gdp->points().entries();
    size_t startPrimIdx = gdp->primitives().entries();
    
    
    GA_PrimitiveGroup * primGrp = buildMesh(getFullName(subd),
                sample.getPositions(), sample.getFaceCounts(),
                        sample.getFaceIndices() );
    
    addUVs(args, subd.getSchema().getUVs(), startPointIdx, startPrimIdx);
    addArbitraryGeomParams(args, subd.getSchema().getArbGeomParams(),
            startPointIdx, startPrimIdx);
    
    //apply xforms via gdp->transform so that we don't have to think
    //about normals and other affected attributes
    if (args.includeXform && parentXform != M44d())
    {
        UT_DMatrix4 dxform(parentXform.x);
        UT_Matrix4 xform(dxform);
        gdp->transform(xform, primGrp);
    }
    
    //subd tagging?
}

//-*****************************************************************************

void SOP_AlembicIn::buildPolyMesh( Args & args, IPolyMesh & polymesh,
        M44d parentXform)
{
    ISampleSelector sampleSelector( args.abcTime );
    IPolyMeshSchema &schema = polymesh.getSchema();
    IPolyMeshSchema::Sample sample = schema.getValue( sampleSelector );
    
    
    //store the primitive and point start indices
    size_t startPointIdx = gdp->points().entries();
    size_t startPrimIdx = gdp->primitives().entries();
    
    GA_PrimitiveGroup * primGrp = buildMesh(getFullName(polymesh),
            sample.getPositions(), sample.getFaceCounts(),
                    sample.getFaceIndices());
    
    addUVs(args, polymesh.getSchema().getUVs(), startPointIdx, startPrimIdx);
    addNormals(args, polymesh.getSchema().getNormals(), startPointIdx,
            startPrimIdx);
    
    addArbitraryGeomParams(args, polymesh.getSchema().getArbGeomParams(),
            startPointIdx, startPrimIdx);
    
    //apply xforms via gdp->transform so that we don't have to think
    //about normals and other affected attributes
    if (args.includeXform && parentXform != M44d())
    {
        UT_DMatrix4 dxform(parentXform.x);
        UT_Matrix4 xform(dxform);
        gdp->transform(xform, primGrp);
    }
    
}

//-*****************************************************************************

GA_PrimitiveGroup * SOP_AlembicIn::buildMesh(
        const std::string & groupName, V3fArraySamplePtr positions,
        Int32ArraySamplePtr counts, Int32ArraySamplePtr indicies)
{
    uint startPointIdx = gdp->points().entries();

    for ( size_t i = 0, e = positions->size(); i < e; ++i )
    {
        GEO_Point *pt = gdp->appendPoint();
        pt->setPos(UT_Vector3(
                (*positions)[i][0],
                (*positions)[i][1],
                (*positions)[i][2]));
    }
    
    GA_PrimitiveGroup *primGrp = 0;
    
    if ( !groupName.empty() )
    {
        primGrp = gdp->newPrimitiveGroup( groupName.c_str() );
    }
    
    size_t npolys = counts->size();
    
    uint32_t currentVtxIndex = 0;
    for ( size_t i = 0; i < npolys; ++i )
    {
        uint32_t numPointsInFace = (*counts)[i];
        
        GU_PrimPoly *poly = GU_PrimPoly::build(gdp,
                numPointsInFace, GU_POLY_CLOSED, 0);
        
        for ( uint32_t ptN = 0; ptN < numPointsInFace;
                ++ptN, ++currentVtxIndex )
        {
            GEO_Point *point = gdp->points()(
                    (*indicies)[currentVtxIndex] + startPointIdx);
            poly->getVertex(ptN).setPt(point);
        }
        
        if ( primGrp )
        {
            primGrp->add( poly->castToGeo() );
        }
    }
    
    return primGrp;
}

//-*****************************************************************************

void
newSopOperator(OP_OperatorTable *table)
{
    table->addOperator(new OP_Operator(
        "Alembic_In",             // Internal name
        "Alembic_In",            // GUI name
        SOP_AlembicIn::myConstructor,   // Op Constructr
        SOP_AlembicIn::myTemplateList,  // GUI Definition
        0,0,                         // Min,Max # of Inputs
        0,OP_FLAG_GENERATOR)         // Local Variables/Generator
    );
}
