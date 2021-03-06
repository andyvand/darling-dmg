#include "HFSExtentsOverflowBTree.h"
#include "be.h"
#include <stdexcept>

HFSExtentsOverflowBTree::HFSExtentsOverflowBTree(std::shared_ptr<HFSFork> fork, CacheZone* zone)
	: HFSBTree(fork, zone, "ExtentsOverflow")
{
}

void HFSExtentsOverflowBTree::findExtentsForFile(HFSCatalogNodeID cnid, bool resourceFork, uint32_t startBlock, std::vector<HFSPlusExtentDescriptor>& extraExtents)
{
	HFSPlusExtentKey key;
	std::vector<HFSBTreeNode> leaves;
	bool first = true;

	key.forkType = resourceFork ? 0xff : 0;
	key.fileID = htobe32(cnid);

	leaves = findLeafNodes((Key*) &key, cnidComparator);

	for (const HFSBTreeNode& leaf : leaves)
	{
		for (int i = 0; i < leaf.recordCount(); i++)
		{
			HFSPlusExtentKey* recordKey = leaf.getRecordKey<HFSPlusExtentKey>(i);
			HFSPlusExtentDescriptor* extents;

			if (recordKey->forkType != key.forkType || be(recordKey->fileID) != cnid)
				continue;
			
			//std::cout << "Examining extra extents from startBlock " << be(recordKey->startBlock) << std::endl;
			if (be(recordKey->startBlock) < startBlock) // skip descriptors already contained in the extents file
				continue;

			if (first)
			{
				if (be(recordKey->startBlock) != startBlock)
					throw std::runtime_error("Unexpected startBlock value");
				first = false;
			}

			extents = leaf.getRecordData<HFSPlusExtentDescriptor>(i);

			// up to 8 extent descriptors per record
			for (int x = 0; x < 8; x++)
			{
				if (!extents[x].blockCount)
				{
					//std::cout << "Extent #" << x << " has zero blockCount\n";
					break;
				}

				extraExtents.push_back(HFSPlusExtentDescriptor{ be(extents[x].startBlock), be(extents[x].blockCount) });
			}
		}
	}
}

int HFSExtentsOverflowBTree::cnidComparator(const Key* indexKey, const Key* desiredKey)
{
	const HFSPlusExtentKey* indexExtentKey = reinterpret_cast<const HFSPlusExtentKey*>(indexKey);
	const HFSPlusExtentKey* desiredExtentKey = reinterpret_cast<const HFSPlusExtentKey*>(desiredKey);

	if (indexExtentKey->forkType > desiredExtentKey->forkType)
		return 1;
	else if (indexExtentKey->forkType < desiredExtentKey->forkType)
		return -1;
	else
	{
		if (be(indexExtentKey->fileID) > be(desiredExtentKey->fileID))
			return 1;
		else if (be(indexExtentKey->fileID) < be(desiredExtentKey->fileID))
			return -1;
		else
			return 0;
	}
}

