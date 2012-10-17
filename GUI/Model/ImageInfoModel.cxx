#include "ImageInfoModel.h"
#include "LayerAssociation.txx"
#include "MetaDataAccess.h"
#include <cctype>


// This compiles the LayerAssociation for the color map
template class LayerAssociation<ImageInfoLayerProperties,
                                GreyImageWrapperBase,
                                ImageInfoModelBase::PropertiesFactory>;

ImageInfoModel::ImageInfoModel()
{
  m_ImageDimensionsModel = makeChildPropertyModel(
        this, &Self::GetImageDimensions);

  m_ImageSpacingModel = makeChildPropertyModel(
        this, &Self::GetImageSpacing);

  m_ImageOriginModel = makeChildPropertyModel(
        this, &Self::GetImageOrigin);

  m_ImageItkCoordinatesModel = makeChildPropertyModel(
        this, &Self::GetImageItkCoordinates);

  m_ImageNiftiCoordinatesModel = makeChildPropertyModel(
        this, &Self::GetImageNiftiCoordinates);

  m_ImageMinMaxModel = makeChildPropertyModel(
        this, &Self::GetImageMinMax);

  m_ImageOrientationModel = makeChildPropertyModel(
        this, &Self::GetImageOrientation);

  // Create the property model for the filter
  m_MetadataFilterModel = ConcreteSimpleStringProperty::New();

  // Listen to events on the filter, so we can update the metadata
  Rebroadcast(m_MetadataFilterModel, ValueChangedEvent(), MetadataChangeEvent());

  // Also rebroadcast active layer change events as both ModelChange and Metadata
  // change events
  Rebroadcast(this, ActiveLayerChangedEvent(), MetadataChangeEvent());
}

void ImageInfoModel::SetParentModel(GlobalUIModel *parent)
{
  Superclass::SetParentModel(parent);

  // Cursor update events are mapped to model update events
  Rebroadcast(m_ParentModel, CursorUpdateEvent(), ModelUpdateEvent());
}


void ImageInfoModel::RegisterWithLayer(ScalarImageWrapperBase *layer)
{
  // We don't need to listen to the events on the layer because they
  // are not going to change anything managed by this model.
}

void ImageInfoModel::UnRegisterFromLayer(ScalarImageWrapperBase *layer)
{
  // We don't need to listen to the events on the layer because they
  // are not going to change anything managed by this model.
}

bool ImageInfoModel
::GetImageDimensions(Vector3ui &value)
{
  if(!this->GetLayer()) return false;
  value = GetLayer()->GetSize();
  return true;
}

bool ImageInfoModel
::GetImageOrigin(Vector3d &value)
{
  if(!this->GetLayer()) return false;
  value = GetLayer()->GetImageBase()->GetOrigin();
  return true;
}

bool ImageInfoModel
::GetImageSpacing(Vector3d &value)
{
  if(!this->GetLayer()) return false;
  value = GetLayer()->GetImageBase()->GetSpacing();
  return true;
}

bool ImageInfoModel
::GetImageItkCoordinates(Vector3d &value)
{
  if(!this->GetLayer()) return false;
  Vector3ui cursor = m_ParentModel->GetDriver()->GetCursorPosition();
  value = GetLayer()->TransformVoxelIndexToPosition(cursor);
  return true;
}

bool ImageInfoModel
::GetImageNiftiCoordinates(Vector3d &value)
{
  if(!this->GetLayer()) return false;
  Vector3ui cursor = m_ParentModel->GetDriver()->GetCursorPosition();
  value = GetLayer()->TransformVoxelIndexToNIFTICoordinates(to_double(cursor));
  return true;
}

bool ImageInfoModel
::GetImageMinMax(Vector2d &value)
{
  if(!this->GetLayer()) return false;
  value = Vector2d(GetLayer()->GetImageMinNative(),
                   GetLayer()->GetImageMaxNative());
  return true;
}

bool ImageInfoModel
::GetImageOrientation(std::string &value)
{
  if(!this->GetLayer()) return false;

  const ImageCoordinateGeometry &geo =
      m_ParentModel->GetDriver()->GetCurrentImageData()->GetImageGeometry();
  ImageCoordinateGeometry::DirectionMatrix dmat =
      geo.GetImageDirectionCosineMatrix();

  std::string raicode =
    ImageCoordinateGeometry::ConvertDirectionMatrixToClosestRAICode(dmat);

  if (ImageCoordinateGeometry::IsDirectionMatrixOblique(dmat))
    value = std::string("Oblique (closest to ") + raicode + string(")");
  else
    value = raicode;

  return true;
}

void ImageInfoModel::OnUpdate()
{
  Superclass::OnUpdate();

  // Is there a change to the metadata?
  if(this->m_EventBucket->HasEvent(ActiveLayerChangedEvent()) ||
     this->m_EventBucket->HasEvent(ValueChangedEvent()))
    {
    // Recompute the metadata index
    this->UpdateMetadataIndex();
    }
}

// #include <itksys/RegularExpression.hxx>

bool case_insensitive_predicate(char a, char b)
{
  return std::tolower(a) == std::tolower(b);
}

bool case_insensitive_find(std::string &a, std::string &b)
{
  std::string::iterator it = std::search(
        a.begin(), a.end(), b.begin(), b.end(), case_insensitive_predicate);
  return it != a.end();
}

void ImageInfoModel::UpdateMetadataIndex()
{
  // Clear the list of selected keys
  m_MetadataKeys.clear();

  // Search keys and values that meet the filter
  if(GetLayer())
    {
    MetaDataAccess mda(GetLayer()->GetImageBase());
    std::vector<std::string> keys = mda.GetKeysAsArray();
    std::string filter = m_MetadataFilterModel->GetValue();
    for(size_t i = 0; i < keys.size(); i++)
      {
      std::string key = keys[i];
      std::string dcm = mda.MapKeyToDICOM(key);
      std::string value = mda.GetValueAsString(key);

      if(filter.size() == 0 ||
         case_insensitive_find(dcm, filter) ||
         case_insensitive_find(value, filter))
        {
        m_MetadataKeys.push_back(key);
        }
      }
    }
}

int ImageInfoModel::GetMetadataRows()
{
  return m_MetadataKeys.size();
}

std::string ImageInfoModel::GetMetadataCell(int row, int col)
{
  assert(GetLayer());
  assert(row >= 0 && row < (int) m_MetadataKeys.size());
  std::string key = m_MetadataKeys[row];
  MetaDataAccess mda(GetLayer()->GetImageBase());

  return (col == 0) ? mda.MapKeyToDICOM(key) : mda.GetValueAsString(key);
}



