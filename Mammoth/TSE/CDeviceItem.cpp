//	CDeviceItem.cpp
//
//	CDeviceItem class
//	Copyright (c) 2019 Kronosaur Productions, LLC. All Rights Reserved.

#include "PreComp.h"

void CDeviceItem::AccumulateAttributes (const CItem &Ammo, TArray<SDisplayAttribute> *retList) const

//	AccumulateAttributes
//
//	Accumulate display attributes.

	{
	GetDeviceClass().AccumulateAttributes(*this, Ammo, retList);
	}

TSharedPtr<CItemEnhancementStack> CDeviceItem::GetEnhancementStack (void) const

//	GetEnhancementStack
//
//	Returns the enhancement stack for this item. If there are no enhancements, 
//	we return NULL.

	{
	//	If we have installed armor, then get the enhancement stack from it.

	if (const CInstalledDevice *pInstalled = m_pCItem->GetInstalledDevice())
		return pInstalled->GetEnhancementStack();

	//	Otherwise, see if we've got a cached enhancement stack

	if (m_pEnhancements)
		return m_pEnhancements;

	//	Otherwise, we need to create one from mods

	const CItemEnhancement &Mods = m_pCItem->GetMods();
	if (Mods.IsEmpty())
		return NULL;

	m_pEnhancements.TakeHandoff(new CItemEnhancementStack);
	m_pEnhancements->Insert(Mods);
	return m_pEnhancements;
	}

DWORD CDeviceItem::GetLinkedFireOptions (void) const

//	GetLinkedFireOptions
//
//	Returns linked fire options.

	{
	//	Options from enhancements

	DWORD dwOptions = GetEnhancements().GetLinkedFireOptions();

	//	Options from the device slot

	if (const CInstalledDevice *pInstalled = m_pCItem->GetInstalledDevice())
		dwOptions = CDeviceClass::CombineLinkedFireOptions(dwOptions, pInstalled->GetSlotLinkedFireOptions());

	//	Options from device class

	dwOptions = CDeviceClass::CombineLinkedFireOptions(dwOptions, GetDeviceClass().GetLinkedFireOptions());
	
	//	Done

	return dwOptions;
	}
