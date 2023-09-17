// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherDetailUtils.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "MaterialList.h"
#include "TetherCableProperties.h"
#include "Mesh/CableMeshGenerationType.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyPathHelpers.h"
#include "Misc/EngineVersionComparison.h"

struct FTetherCableProperties;

#if UE_VERSION_OLDER_THAN(4,25,0)
typedef UObjectProperty FObjectProperty;
#endif

TArray<TSharedPtr<class IPropertyHandle>> FTetherDetailUtils::FindPropertyLeaves(const TSharedPtr<IPropertyHandle> Property)
{
	uint32 NumChildren;
	Property->GetNumChildren(NumChildren);

	// If no children, self is the only leaf
	if(NumChildren == 0)
	{
		return { Property };
	}

	// Find leaves of all children
	TArray<TSharedPtr<class IPropertyHandle>> Result;
	for(uint32 i = 0; i<NumChildren;i++)
	{
		const TSharedPtr<class IPropertyHandle> Child = Property->GetChildHandle(i);
		if(Child->GetProperty()->IsA(FObjectProperty::StaticClass()))
		{
			Result.Add(Child);
		}
		else
		{
			Result.Append(FindPropertyLeaves(Child));
		}
	}
	return Result;
}

void FTetherDetailUtils::FlattenPropertyTree(IDetailLayoutBuilder& DetailBuilder,
	const TSharedPtr<IPropertyHandle> Property)
{
	// Add leaf properties
	TArray< TSharedPtr<IPropertyHandle>> Leaves = FindPropertyLeaves(Property);
	for (const TSharedPtr<IPropertyHandle> Leaf : Leaves)
	{
		DetailBuilder.AddPropertyToCategory(Leaf);
	}
	// Remove original property
	DetailBuilder.HideProperty(Property);
}

void FTetherDetailUtils::ReplacePropertyWithChildren(IDetailLayoutBuilder& DetailBuilder, const TSharedPtr<IPropertyHandle> Property, TArray<TSharedPtr<class IPropertyHandle>> ExcludedProps)
{
	// Add children
	uint32 NumChildren;
	Property->GetNumChildren(NumChildren);
	for(uint32 i=0; i< NumChildren;i++)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = Property->GetChildHandle(i);
		if(ExcludedProps.ContainsByPredicate([ChildHandle](TSharedPtr<IPropertyHandle> ExcludedProp)
		{
			return ExcludedProp->GetProperty() == ChildHandle->GetProperty();
		}) )
		{
			continue;
		}
		DetailBuilder.AddPropertyToCategory(ChildHandle);
	}
	// Remove original property
	DetailBuilder.HideProperty(Property);
}

TSharedRef<FMaterialList> FTetherDetailUtils::CreateCablePropertiesMaterialList(class IDetailLayoutBuilder& DetailBuilder, TArray<FCablePropertiesPropertyInfo> CableProperties)
{
	FMaterialListDelegates MaterialListDelegates;
	MaterialListDelegates.OnGetMaterials.BindLambda([CableProperties](class IMaterialListBuilder& OutMaterials)
    {
		for(const FCablePropertiesPropertyInfo& PropertyInfo : CableProperties)
		{
			if(IsValid(PropertyInfo.Object))
			{
				FTetherCableProperties* Properties = PropertyInfo.Property->GetProperty()->ContainerPtrToValuePtr<FTetherCableProperties>(PropertyInfo.Object);
				const UTetherMeshGenerator* MeshGenCDO = FCableMeshGenerationType::GetMeshGeneratorClass(Properties->MeshType)->GetDefaultObject<UTetherMeshGenerator>();
				const int32 NumMaterials = MeshGenCDO->GetNumMaterials(*Properties);
				
				for(int32 i=0; i<NumMaterials; i++)
				{
					UMaterialInterface* Material = MeshGenCDO->GetMaterial(*Properties, i);
					UMaterialInterface* DefaultMaterial = MeshGenCDO->GetDefaultMaterial(*Properties, i);
                    OutMaterials.AddMaterial(i, Material, Material != DefaultMaterial);
                }
			}

		}
		
    });

	MaterialListDelegates.OnMaterialChanged.BindLambda([CableProperties](UMaterialInterface* NewMaterial, UMaterialInterface* PrevMaterial, int32 SlotIndex, bool bReplaceAll)
	{
		for(const FCablePropertiesPropertyInfo& PropertyInfo : CableProperties)
		{
			if(IsValid(PropertyInfo.Object))
			{
				ensure(PropertyInfo.Property.IsValid());
				FProperty* Property = PropertyInfo.Property->GetProperty();
				ensure(Property);
				FTetherCableProperties* Properties = Property->ContainerPtrToValuePtr<FTetherCableProperties>(PropertyInfo.Object);
				const UTetherMeshGenerator* MeshGenCDO = FCableMeshGenerationType::GetMeshGeneratorClass(Properties->MeshType)->GetDefaultObject<UTetherMeshGenerator>();
				const int32 NumMaterials = MeshGenCDO->GetNumMaterials(*Properties);

				if(NumMaterials > SlotIndex)
				{
					UMaterialInterface* Material = MeshGenCDO->GetMaterial(*Properties, SlotIndex);
					if(Material == PrevMaterial)
					{
						PropertyInfo.Object->Modify();

						FPropertyAccessChangeNotify Notify;
						Notify.ChangedObject = PropertyInfo.Object;
						Notify.ChangeType = EPropertyChangeType::ValueSet;
						Notify.NotifyMode = EPropertyAccessChangeNotifyMode::Always;
						const FCachedPropertyPath Path(Property->GetName() + "." + GET_MEMBER_NAME_CHECKED(FTetherCableProperties, Materials).ToString());
						Path.Resolve(PropertyInfo.Object);
						Path.ToEditPropertyChain(Notify.ChangedPropertyChain);

						PropertyAccessUtil::EmitPreChangeNotify(&Notify, false);
						
						TArray<UMaterialInterface*>& MaterialArray = Properties->Materials;
						if(!MaterialArray.IsValidIndex(SlotIndex))
						{
							MaterialArray.SetNum(SlotIndex + 1, false);
						}

						MaterialArray[SlotIndex] = NewMaterial;

						PropertyAccessUtil::EmitPostChangeNotify(&Notify, false);
					}
				}
            }
		}
	});
	
	const TArray<FAssetData> MaterialListOwner;
#if UE_VERSION_OLDER_THAN(5,0,0)
	return MakeShareable(new FMaterialList(DetailBuilder, MaterialListDelegates, MaterialListOwner, false, true, true));
#else
	return MakeShareable(new FMaterialList(DetailBuilder, MaterialListDelegates, MaterialListOwner, false, true));
#endif
}
