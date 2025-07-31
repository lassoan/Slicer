/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Children's Hospital of Philadelphia, USA. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Csaba Pinter, Ebatinca, funded
  by the grant GRT-00000485 of Children's Hospital of Philadelphia, USA.

==============================================================================*/

///  vtkMRMLVolumeSequenceStorageNode - MRML node that can read/write
///  a Sequence node containing volumes in a single NRRD file.

#ifndef __vtkMRMLVolumeSequenceStorageNode_h
#define __vtkMRMLVolumeSequenceStorageNode_h

#include "vtkMRML.h"

#include "vtkMRMLNRRDStorageNode.h"
#include <string>

class VTK_MRML_EXPORT vtkMRMLVolumeSequenceStorageNode : public vtkMRMLNRRDStorageNode
{
public:
  static vtkMRMLVolumeSequenceStorageNode* New();
  vtkTypeMacro(vtkMRMLVolumeSequenceStorageNode, vtkMRMLNRRDStorageNode);

  vtkMRMLNode* CreateNodeInstance() override;

  ///
  /// Get node XML tag name (like Storage, Model)
  const char* GetNodeTagName() override { return "VolumeSequenceStorage"; };

  /// Return true if the node can be read in.
  bool CanReadInReferenceNode(vtkMRMLNode* refNode) override;

  /// Return true if the node can be written by using the writer.
  bool CanWriteFromReferenceNode(vtkMRMLNode* refNode) override;

  /// Convert voxel vector type enum from VTKITK type to MRML type
  int ConvertVoxelVectorTypeVTKITKToMRML(int vtkitkType);
  /// Convert voxel vector type enum from MRML type to VTKITK type
  int ConvertVoxelVectorTypeMRMLToVTKITK(int mrmlType);

  /// Write the data. Returns 1 on success, 0 otherwise.
  ///
  /// The nrrd file will be formatted such as:
  /// "kinds: [component] domain domain domain list"
  int WriteDataInternal(vtkMRMLNode* refNode) override;

  ///
  /// Return a default file extension for writing
  const char* GetDefaultWriteFileExtension() override;

protected:
  vtkMRMLVolumeSequenceStorageNode();
  ~vtkMRMLVolumeSequenceStorageNode() override;
  vtkMRMLVolumeSequenceStorageNode(const vtkMRMLVolumeSequenceStorageNode&);
  void operator=(const vtkMRMLVolumeSequenceStorageNode&);

  /// Does the actual reading. Returns 1 on success, 0 otherwise.
  /// Returns 0 by default (read not supported).
  /// This implementation delegates most everything to the superclass
  /// but it has an early exit if the file to be read is incompatible.
  ///
  /// It is assumed that the nrrd file is formatted such as:
  /// "kinds: [component] domain domain domain list"
  int ReadDataInternal(vtkMRMLNode* refNode) override;

  /// Initialize all the supported write file types
  void InitializeSupportedReadFileTypes() override;

  /// Initialize all the supported write file types
  void InitializeSupportedWriteFileTypes() override;
};

#endif
