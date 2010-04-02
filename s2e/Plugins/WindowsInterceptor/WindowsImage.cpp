#include "WindowsMonitor.h"
#include "WindowsImage.h"

#include <s2e/Utils.h>
#include <s2e/QemuKleeGlue.h>

using namespace std;
using namespace s2e;
using namespace plugins;

bool WindowsImage::IsValidString(const char *str)
{
  for (unsigned i=0; str[i]; i++) {
    if (str[i] > 0x20 && (unsigned)str[i] < 0x80) {
      continue;
    }
    return false;
  }
  return true;
}

WindowsImage::WindowsImage(uint64_t Base)
{
  m_Base = Base;
  
  if (QEMU::ReadVirtualMemory(m_Base, &DosHeader, sizeof(DosHeader))<0) {
		DPRINTF("Could not load IMAGE_DOS_HEADER structure (m_Base=%#"PRIx64")\n", m_Base);
    return;
	}

	if (DosHeader.e_magic != IMAGE_DOS_SIGNATURE)  {
		DPRINTF("PE image has invalid magic\n");
    return;
	}

	if (QEMU::ReadVirtualMemory(m_Base+DosHeader.e_lfanew, &NtHeader, sizeof(NtHeader))<0) {
		DPRINTF("Could not load IMAGE_NT_HEADER structure (m_Base=%#"PRIx64")\n", m_Base+(unsigned)DosHeader.e_lfanew);
    return;
	}

	if (NtHeader.Signature != IMAGE_NT_SIGNATURE) {
		DPRINTF("NT header has invalid magic\n");
    return;
	}

  m_ImageSize = NtHeader.OptionalHeader.SizeOfImage;
  m_ImageBase = NtHeader.OptionalHeader.ImageBase;
  m_EntryPoint = NtHeader.OptionalHeader.AddressOfEntryPoint;

  InitExports();
  InitImports();
}

int WindowsImage::InitExports()
{
  
    s2e::windows::PIMAGE_DATA_DIRECTORY ExportDataDir;
	uint32_t ExportTableAddress;
	uint32_t ExportTableSize;

	s2e::windows::PIMAGE_EXPORT_DIRECTORY ExportDir;

	unsigned i;
  int res = 0;
  unsigned TblSz;
  uint32_t *Names;
  uint32_t *FcnPtrs;

	ExportDataDir =  &NtHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	ExportTableAddress = ExportDataDir->VirtualAddress + m_Base;
	ExportTableSize = ExportDataDir->Size;
  DPRINTF("ExportTableAddress=%#x ExportTableSize=%#x\n", ExportDataDir->VirtualAddress, ExportDataDir->Size);

  if (!ExportDataDir->VirtualAddress || !ExportTableSize) {
    return -1;
  }

  ExportDir = (s2e::windows::PIMAGE_EXPORT_DIRECTORY)malloc(ExportTableSize);

  if (QEMU::ReadVirtualMemory(ExportTableAddress, (uint8_t*)ExportDir, ExportTableSize)<0) {
		DPRINTF("Could not load PIMAGE_EXPORT_DIRECTORY structures (m_Base=%#x)\n", ExportTableAddress);
		res = -5;
    goto err2;
	}

  TblSz = ExportDir->NumberOfFunctions * sizeof(uint32_t);
  Names = (uint32_t*)malloc(TblSz);
  if (!Names) {
    return -1;
  }

  FcnPtrs = (uint32_t*)malloc(TblSz);
  if (!FcnPtrs) {
    free(Names);
    return -1;
  }
  
  if (QEMU::ReadVirtualMemory(m_Base + ExportDir->AddressOfNames, Names, TblSz)<0) {
		DPRINTF("Could not load names of exported functions");
		res = -6;
    goto err2;
	}

  if (QEMU::ReadVirtualMemory(m_Base + ExportDir->AddressOfFunctions, FcnPtrs, TblSz)<0) {
		DPRINTF("Could not load addresses of  exported functions");
		res = -7;
    goto err3;
  }

  for (i=0; i<ExportDir->NumberOfFunctions; i++) {
    string FunctionName;
    char *CFcnName;
    
    uint32_t EffAddr = Names[i] + m_Base;
    if (EffAddr < m_Base || EffAddr >= m_Base + m_ImageSize) {
        continue;
    }
    
    CFcnName = QEMU::GetAsciiz(Names[i] + m_Base);
    if (!CFcnName || !IsValidString(CFcnName)) {
      continue;
    }
    FunctionName = CFcnName;
    free(CFcnName);

    DPRINTF("Export %s @%#"PRIx64"\n", FunctionName.c_str(), FcnPtrs[i]+m_Base);
    m_Exports[FunctionName] = FcnPtrs[i];
  }

      free(FcnPtrs);
err3: free(Names);
err2: free(ExportDir);
//err1: 
  return res;
}

int WindowsImage::InitImports()
{
	s2e::windows::PIMAGE_DATA_DIRECTORY ImportDir;
	uint64_t ImportTableAddress;
	uint32_t ImportTableSize;

	uint64_t ImportNameTable;
	uint64_t ImportAddressTable;

	s2e::windows::PIMAGE_IMPORT_DESCRIPTOR ImportDescriptors;
	unsigned ImportDescCount;
	unsigned i, j;

	ImportDir =  &NtHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  ImportTableAddress = ImportDir->VirtualAddress + m_Base;
	ImportTableSize = ImportDir->Size;

  if (!ImportTableAddress || !ImportTableSize) {
    return -1;
  }

	ImportDescCount = ImportTableSize / sizeof(s2e::windows::IMAGE_IMPORT_DESCRIPTOR);
	ImportDescriptors = (s2e::windows::PIMAGE_IMPORT_DESCRIPTOR)malloc(ImportTableSize);
	if (!ImportDescriptors) {
		DPRINTF("Could not allocate memory for import descriptors\n");
		return -5;
	}

	if (QEMU::ReadVirtualMemory(ImportTableAddress, ImportDescriptors, ImportTableSize)<0) {
		DPRINTF("Could not load IMAGE_IMPORT_DESCRIPTOR structures (base=%#"PRIx64")\n", ImportTableAddress);
		free(ImportDescriptors);
		return -6;
	}

	for (i=0; ImportDescriptors[i].Characteristics && i<ImportDescCount; i++) {
		s2e::windows::IMAGE_THUNK_DATA32 INaT;
		char *CDllName;
    if (!(CDllName = QEMU::GetAsciiz(ImportDescriptors[i].Name + m_Base))) {
      continue;
    }
    if (!IsValidString(CDllName)) {
      continue;
    }

		DPRINTF("%s\n", CDllName);

    string DllName = CDllName;
    free(CDllName);
		
		ImportAddressTable = m_Base + ImportDescriptors[i].FirstThunk;
		ImportNameTable = m_Base + ImportDescriptors[i].OriginalFirstThunk;

		j=0;
		do {
			s2e::windows::IMAGE_THUNK_DATA32 IAT;
			uint32_t Name;
			int res1, res2;
			res1 = QEMU::ReadVirtualMemory(ImportAddressTable+j*sizeof(s2e::windows::IMAGE_THUNK_DATA32), 
				&IAT, sizeof(IAT));
			res2 = QEMU::ReadVirtualMemory(ImportNameTable+j*sizeof(s2e::windows::IMAGE_THUNK_DATA32), 
				&INaT, sizeof(INaT));
			
			if (res1 < 0 || res2 < 0) {
				DPRINTF("Could not load IAT entries\n");
				free(ImportDescriptors);
				return -7;
			}

			if (!INaT.u1.AddressOfData)
				break;

			if (INaT.u1.AddressOfData & IMAGE_ORDINAL_FLAG) {
				uint32_t Tmp = INaT.u1.AddressOfData & ~0xFFFF;
				Tmp &= ~IMAGE_ORDINAL_FLAG;
				if (!Tmp) {
					DPRINTF("Does not support import by ordinals\n");
					break;
				}
			}else {
				INaT.u1.AddressOfData += m_Base;
			}

			string FunctionName;
      char *CFcnName;
			Name = INaT.u1.AddressOfData;
			
      if (Name < m_Base || Name >= m_Base + m_ImageSize) {
        j++;
        continue;
      }

      CFcnName = QEMU::GetAsciiz(Name+2);
      if (!CFcnName || !IsValidString(CFcnName)) {
        j++;
        continue;
      }
      FunctionName = CFcnName;
      free(CFcnName);
			DPRINTF("  %s @%#x\n", FunctionName.c_str(), IAT.u1.Function);
			
      ImportedFunctions &ImpFcnIt = m_Imports[DllName];
      ImpFcnIt[FunctionName] = IAT.u1.Function;
      
      /* Check if we already hooked the given address */
      //iohook_winstrucs_hook_import(FunctionName, IAT.u1.Function);
			j++;
		}while(INaT.u1.AddressOfData);
	}
	
	free(ImportDescriptors);
	return 0;

}


void WindowsImage::DumpInfo(std::iostream &os) const
{
  /* Printing info about all imported functions */
  foreach2(it, m_Imports.begin(), m_Imports.end()) {
    const IExecutableImage::ImportedFunctions &f = (*it).second;
    foreach2(it2, f.begin(), f.end()) {
      os << "IMP " << (*it2).first << " " 
        << hex << (*it2).second << endl;
    }
  }
}