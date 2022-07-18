#include <UE/structs.h>

// HAHAHAHGHAHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH

struct FFortItemEntry : FFastArraySerializerItem // My implementation // ALWAYS BE A POINTER 
{
	FGuid GetGuid()
	{
		static auto ItemGuidOffset = FindOffsetStruct(_("ScriptStruct /Script/FortniteGame.FortItemEntry"), _("ItemGuid"));
		// ItemGuidOffset = 0x0050;
		return *(FGuid*)(__int64(this) + ItemGuidOffset);
	}

	static int GetStructSize()
	{
		return 0xD0;
	}
};


namespace Inventory
{
	TArray<UObject*>* GetItemInstances(UObject* Controller)
	{
		static auto ItemInstancesOffset = FindOffsetStruct(_("ScriptStruct /Script/FortniteGame.FortItemList"), _("ItemInstances"));

		auto WorldInventory = *Controller->Member<UObject*>(_("WorldInventory"));
		auto Inventory = WorldInventory->Member<void>(_("Inventory"));

		// ItemInstancesOffset = 0x110;

		return (TArray<UObject*>*)(__int64(Inventory) + ItemInstancesOffset);
	}

	TArray<FFortItemEntry>* GetReplicatedEntries(UObject* Controller)
	{
		static auto ReplicatedEntriesOffset = FindOffsetStruct(_("ScriptStruct /Script/FortniteGame.FortItemList"), _("ReplicatedEntries"));

		auto WorldInventory = *Controller->Member<UObject*>(_("WorldInventory"));
		auto Inventory = WorldInventory->Member<void>(_("Inventory"));
		// ReplicatedEntriesOffset = 0xB0;

		return (TArray<FFortItemEntry>*)(__int64(Inventory) + ReplicatedEntriesOffset);
	}

	UObject* GetQuickBars(UObject* Controller)
	{
		static auto FnVerDouble = std::stod(FN_Version);

		if (FnVerDouble <= 7.30) // No client Quickbars
		{
			return *Controller->Member<UObject*>(_("QuickBars"));
		}

		return nullptr;
	}

	inline UObject* EquipWeaponDefinition(UObject* Pawn, UObject* Definition, FGuid& Guid, int Ammo = 0)
	{
		// auto weaponClass = Definition->GetWeaponActorClass();
		// if (weaponClass)
		{
			// auto Weapon = (AFortWeapon*)SpawnActorTrans(weaponClass, {}, Pawn);
			static auto equipFn = Pawn->Function(_("EquipWeaponDefinition"));
			struct {
				UObject* Def;
				FGuid Guid;
				UObject* Wep;
			} params{Definition, Guid};
			Pawn->ProcessEvent(equipFn, &params);
			auto Weapon = params.Wep;
			if (Weapon)
			{
				*Weapon->Member<UObject*>(_("WeaponData")) = Definition;
				*Weapon->Member<FGuid>(_("ItemEntryGuid")) = Guid;

				// if (bEquipWithMaxAmmo)
				{
					static auto getBulletsFn = Weapon->Function(_("GetBulletsPerClip"));
					if (getBulletsFn)
					{
						int BulletsPerClip;
						Weapon->ProcessEvent(getBulletsFn, &BulletsPerClip);
						*Weapon->Member<int>(_("AmmoCount")) = BulletsPerClip;
					}
					else
						std::cout << _("No GetBulletsPerClip!\n");
				}

				// Instance->ItemEntry.LoadedAmmo = Weapon->AmmoCount;

				Helper::SetOwner(Weapon, Pawn);
				Weapon->ProcessEvent(_("OnRep_ReplicatedWeaponData"));
				Weapon->ProcessEvent(_("OnRep_AmmoCount"));

				struct { UObject* P; } givenParams{ Pawn };

				static auto givenFn = Weapon->Function(_("ClientGivenTo"));
				if (givenFn)
					givenFn->ProcessEvent(givenFn, &givenParams);
				else
					std::cout << _("No ClientGivenTo!\n");

				struct { UObject* Weapon; } internalEquipParams{ Weapon };

				static auto internalEquipFn = Pawn->Function(_("ClientInternalEquipWeapon"));
				if (internalEquipFn)
					Pawn->ProcessEvent(internalEquipFn, &internalEquipParams);
				else
					std::cout << _("No ClientInternalEquipWeapon!\n");

				// Pawn->OnRep_CurrentWeapon(); // i dont think this is needed but alr
			}
			else
				std::cout << _("No weapon!\n");

			return Weapon;
		}

		return nullptr;
	}

	void Update(UObject* Controller, int Idx = -1, bool bRemovedItem = false)
	{
		auto QuickBars = GetQuickBars(Controller);

		auto WorldInventory = *Controller->Member<UObject*>(_("WorldInventory"));
		auto Inventory = WorldInventory->Member<void>(_("Inventory"));

		static auto WorldHandleInvUpdate = WorldInventory->Function(_("HandleInventoryLocalUpdate"));

		if (WorldHandleInvUpdate)
		{
			WorldInventory->ProcessEvent(WorldHandleInvUpdate, nullptr);

			static auto PCHandleInvUpdate = Controller->Function(_("HandleWorldInventoryLocalUpdate"));

			if (PCHandleInvUpdate)
				Controller->ProcessEvent(PCHandleInvUpdate);	
		}

		// static auto OnRep_QuickBar = Controller->Function(_("OnRep_QuickBar"));
		// Controller->ProcessEvent(OnRep_QuickBar, nullptr);

		if (QuickBars)
		{
			static auto OnRep_PrimaryQuickBar = QuickBars->Function(_("OnRep_PrimaryQuickBar"));
			QuickBars->ProcessEvent(OnRep_PrimaryQuickBar);

			static auto OnRep_SecondaryQuickBar = QuickBars->Function(_("OnRep_SecondaryQuickBar"));
			QuickBars->ProcessEvent(OnRep_SecondaryQuickBar);
		}

		if (bRemovedItem)
			((FFastArraySerializer*)Inventory)->MarkArrayDirty();

		if (Idx != -1)
			((FFastArraySerializer*)Inventory)->MarkItemDirty(GetReplicatedEntries(Controller)->At(Idx));
	}

	static FFortItemEntry* AddItemToSlot(UObject* Controller, UObject* Definition, int Slot, EFortQuickBars QuickBarsType = EFortQuickBars::Primary, int Count = 1)
	{
		// Create an ItemInstance

		auto QuickBars = GetQuickBars(Controller);
		
		if (!QuickBars || !Definition)
			return nullptr;

		struct {
			int                                                Count;                                                    // (Parm, ZeroConstructor, IsPlainOldData)
			int                                                Level;                                                    // (Parm, ZeroConstructor, IsPlainOldData)
			UObject* ReturnValue;
		} CTIIParams{Count, 1};

		static auto CTIIFn = Definition->Function(_("CreateTemporaryItemInstanceBP"));
		Definition->ProcessEvent(CTIIFn, &CTIIParams);

		auto ItemInstance = CTIIParams.ReturnValue; // UFortItem

		if (ItemInstance)
		{
			struct { UObject* InController; } SOCFTIParams{Controller};
			static auto SOCFTIFn = ItemInstance->Function(_("SetOwningControllerForTemporaryItem"));
			ItemInstance->ProcessEvent(SOCFTIFn, &SOCFTIParams);

			TArray<UObject*>* ItemInstances = GetItemInstances(Controller);

			TArray<FFortItemEntry>* ReplicatedEntries = GetReplicatedEntries(Controller);

			FFortItemEntry* Entry = ItemInstance->Member<FFortItemEntry>(_("ItemEntry"));

			std::cout << _("Found ItemEntry!\n");

			int Idx = -1;

			if (ReplicatedEntries && Entry)
			{
				Idx = ReplicatedEntries->Add(*Entry, FFortItemEntry::GetStructSize());
				std::cout << _("Added to ReplicatedEntries!\n");

				if (ItemInstances && Idx != -1)
					ItemInstances->Add(ItemInstance, FFortItemEntry::GetStructSize());
				else
					std::cout << _("Unable to find ItemInstances!\n");
			}
			else
				std::cout << _("Unable to find ReplicatedEntries!\n");
			
			static UObject* GetItemGuid = FindObject(_("Function /Script/FortniteGame.FortItem.GetItemGuid"));
			FGuid Guid;

			ItemInstance->ProcessEvent(GetItemGuid, &Guid);

			// auto& Guid = Entry->GetGuid();

			struct {
				FGuid                                       Item;                                                     // (Parm, ZeroConstructor, IsPlainOldData)
				EFortQuickBars                                     InQuickBar;                                               // (Parm, ZeroConstructor, IsPlainOldData)
				int                                                Slot;
			} SAIIParams{Guid, QuickBarsType, Slot};

			std::cout << "GUID A: " << Guid.A << '\n';
			std::cout << "GUID B: " << Guid.B << '\n';
			std::cout << "GUID C: " << Guid.C << '\n';
			std::cout << "GUID D: " << Guid.D << '\n';

			static auto SAIIFn = QuickBars->Function(_("ServerAddItemInternal"));
			QuickBars->ProcessEvent(SAIIFn, &SAIIParams);

			Update(Controller, Idx);

			return Entry;
		}

		return nullptr;
	}

	void RemoveItemFromSlot(UObject* PlayerController)
	{

	}
}