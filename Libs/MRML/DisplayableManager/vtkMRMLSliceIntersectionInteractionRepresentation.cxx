/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMRMLSliceIntersectionInteractionRepresentation.cxx

  Copyright (c) Ebatinca S.L., Las Palmas de Gran Canaria, Spain
  All rights reserved.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkMRMLSliceIntersectionInteractionRepresentation.h"


#include <deque>
#define _USE_MATH_DEFINES
#include <math.h>

#include "vtkMRMLApplicationLogic.h"
#include "vtkMRMLDisplayableNode.h"
#include "vtkMRMLInteractionNode.h"
#include "vtkMRMLModelDisplayNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLSliceLogic.h"
#include "vtkMRMLSliceNode.h"
#include "vtkMRMLSliceCompositeNode.h"

#include "vtkActor2D.h"
#include "vtkArcSource.h"
#include "vtkAppendPolyData.h"
#include "vtkAssemblyPath.h"
#include "vtkCallbackCommand.h"
#include "vtkCamera.h"
#include "vtkCellArray.h"
#include "vtkCommand.h"
#include "vtkConeSource.h"
#include "vtkCoordinate.h"
#include "vtkCursor2D.h"
#include "vtkCylinderSource.h"
#include "vtkGlyph2D.h"
#include "vtkInteractorObserver.h"
#include "vtkLeaderActor2D.h"
#include "vtkLine.h"
#include "vtkLineSource.h"
#include "vtkMath.h"
#include "vtkMatrix3x3.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkPoints.h"
#include "vtkPolyDataAlgorithm.h"
#include "vtkPolyDataMapper2D.h"
#include "vtkProperty2D.h"
#include "vtkPlane.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkSphereSource.h"
#include "vtkTransform.h"
#include "vtkTransformPolyDataFilter.h"
#include "vtkTextMapper.h"
#include "vtkTextProperty.h"
#include "vtkTubeFilter.h"
#include "vtkWindow.h"

// MRML includes
#include <vtkMRMLInteractionEventData.h>

// Visualization modes
enum
  {
  ShowIntersection = 0,
  HideIntersection = 1,
  };

// Handles type
enum
  {
  Arrows = 0,
  Circles = 1,
  };

// Settings
static const double VISUALIZATION_MODE = HideIntersection;
static const double HANDLES_TYPE = Arrows;
static const bool HANDLES_ALWAYS_VISIBLE = false;
static const double OPACITY_RANGE = 1000.0;
static const double FOV_HANDLES_MARGIN = 0.03; // 3% margin
static const double HIDE_INTERSECTION_GAP_SIZE = 0.05; // 5.0% of the slice view width
static const double INTERACTION_SIZE_PIXELS = 20.0;

// Intersection line
static const double INTERSECTION_LINE_RESOLUTION = 50; // default = 8
static const double INTERSECTION_LINE_EXTRA_THICKNESS = 1.0; // extra thickness with respect to normal slice intersection display

// Handles
static const double LINE_POINTS_FILTERING_THRESHOLD = 15.0;
static const double HANDLES_CIRCLE_THETA_RESOLUTION = 100; // default = 8
static const double HANDLES_CIRCLE_PHI_RESOLUTION = 100; // default = 8
static const double SLICEOFSSET_HANDLE_DEFAULT_POSITION[3] = { 0.0,0.0,0.0 };
static const double SLICEOFSSET_HANDLE_DEFAULT_ORIENTATION[3] = { 0.0,1.0,0.0 };
static const double SLICEOFSSET_HANDLE_CIRCLE_RADIUS = 7.0;
static const double SLICEOFSSET_HANDLE_ARROW_RADIUS = 3.0;
static const double SLICEOFSSET_HANDLE_ARROW_LENGTH = 60.0;
static const double SLICEOFSSET_HANDLE_ARROW_TIP_ANGLE = 27; // degrees
static const double ROTATION_HANDLE_DEFAULT_POSITION[3] = { 0.0,0.0,0.0 };
static const double ROTATION_HANDLE_DEFAULT_ORIENTATION[3] = { 0.0,1.0,0.0 };
static const double ROTATION_HANDLE_CIRCLE_RADIUS = 10.0;
static const double ROTATION_HANDLE_ARROW_RADIUS = 3.0;
static const double ROTATION_HANDLE_ARROW_LENGTH = 60.0;
static const double ROTATION_HANDLE_ARROW_TIP_ANGLE = 27; // degrees
static const double TRANSLATION_HANDLE_OUTER_RADIUS = 9.0;
static const double TRANSLATION_HANDLE_INNER_RADIUS = 7.0;

vtkStandardNewMacro(vtkMRMLSliceIntersectionInteractionRepresentation);
vtkCxxSetObjectMacro(vtkMRMLSliceIntersectionInteractionRepresentation, MRMLApplicationLogic, vtkMRMLApplicationLogic);

class SliceIntersectionInteractionDisplayPipeline
{
  public:

    //----------------------------------------------------------------------
    SliceIntersectionInteractionDisplayPipeline()
    {
      // Intersection line 1
      this->IntersectionLine1 = vtkSmartPointer<vtkLineSource>::New();
      this->IntersectionLine1->SetResolution(INTERSECTION_LINE_RESOLUTION);
      this->IntersectionLine1->Update();
      this->IntersectionLine1Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
      this->IntersectionLine1Property = vtkSmartPointer<vtkProperty2D>::New();
      this->IntersectionLine1Actor = vtkSmartPointer<vtkActor2D>::New();
      this->IntersectionLine1Actor->SetVisibility(false); // invisible until slice node is set
      this->IntersectionLine1Mapper->SetInputConnection(this->IntersectionLine1->GetOutputPort());
      this->IntersectionLine1Actor->SetMapper(this->IntersectionLine1Mapper);
      this->IntersectionLine1Actor->SetProperty(this->IntersectionLine1Property);

      // Intersection line 2
      this->IntersectionLine2 = vtkSmartPointer<vtkLineSource>::New();
      this->IntersectionLine2->SetResolution(INTERSECTION_LINE_RESOLUTION);
      this->IntersectionLine2->Update();
      this->IntersectionLine2Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
      this->IntersectionLine2Property = vtkSmartPointer<vtkProperty2D>::New();
      this->IntersectionLine2Actor = vtkSmartPointer<vtkActor2D>::New();
      this->IntersectionLine2Actor->SetVisibility(false); // invisible until slice node is set
      this->IntersectionLine2Mapper->SetInputConnection(this->IntersectionLine2->GetOutputPort());
      this->IntersectionLine2Actor->SetMapper(this->IntersectionLine2Mapper);
      this->IntersectionLine2Actor->SetProperty(this->IntersectionLine2Property);

      // Center sphere
      this->TranslationOuterHandle = vtkSmartPointer<vtkSphereSource>::New();
      this->TranslationOuterHandle->SetRadius(TRANSLATION_HANDLE_OUTER_RADIUS);
      this->TranslationOuterHandle->SetThetaResolution(HANDLES_CIRCLE_THETA_RESOLUTION);
      this->TranslationOuterHandle->SetPhiResolution(HANDLES_CIRCLE_PHI_RESOLUTION);
      this->TranslationOuterHandle->Update();
      this->TranslationOuterHandleMapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
      this->TranslationOuterHandleProperty = vtkSmartPointer<vtkProperty2D>::New();
      this->TranslationOuterHandleActor = vtkSmartPointer<vtkActor2D>::New();
      this->TranslationOuterHandleActor->SetVisibility(false); // invisible until slice node is set
      this->TranslationOuterHandleMapper->SetInputConnection(this->TranslationOuterHandle->GetOutputPort());
      this->TranslationOuterHandleActor->SetMapper(this->TranslationOuterHandleMapper);
      this->TranslationOuterHandleActor->SetProperty(this->TranslationOuterHandleProperty);
      this->TranslationInnerHandle = vtkSmartPointer<vtkSphereSource>::New();
      this->TranslationInnerHandle->SetRadius(TRANSLATION_HANDLE_INNER_RADIUS);
      this->TranslationInnerHandle->SetThetaResolution(HANDLES_CIRCLE_THETA_RESOLUTION);
      this->TranslationInnerHandle->SetPhiResolution(HANDLES_CIRCLE_PHI_RESOLUTION);
      this->TranslationInnerHandle->Update();
      this->TranslationInnerHandleMapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
      this->TranslationInnerHandleProperty = vtkSmartPointer<vtkProperty2D>::New();
      this->TranslationInnerHandleActor = vtkSmartPointer<vtkActor2D>::New();
      this->TranslationInnerHandleActor->SetVisibility(false); // invisible until slice node is set
      this->TranslationInnerHandleMapper->SetInputConnection(this->TranslationInnerHandle->GetOutputPort());
      this->TranslationInnerHandleActor->SetMapper(this->TranslationInnerHandleMapper);
      this->TranslationInnerHandleActor->SetProperty(this->TranslationInnerHandleProperty);

      // Rotation handles
      this->CreateRotationHandles();

      // Slice offset handles
      this->CreateSliceOffsetHandles();

      // Handle points
      this->IntersectionLine1Points = vtkSmartPointer<vtkPolyData>::New();
      this->IntersectionLine2Points = vtkSmartPointer<vtkPolyData>::New();
      this->RotationHandlePoints = vtkSmartPointer<vtkPolyData>::New();
      this->TranslationHandlePoints = vtkSmartPointer<vtkPolyData>::New();
      this->SliceOffsetHandlePoints = vtkSmartPointer<vtkPolyData>::New();
    }

    //----------------------------------------------------------------------
    virtual ~SliceIntersectionInteractionDisplayPipeline()
    {
      this->SetAndObserveSliceLogic(nullptr, nullptr);
    }

    //----------------------------------------------------------------------
    void CreateRotationHandles()
      {
      // Create list of points to store position of handles
      this->RotationHandle1Points = vtkSmartPointer<vtkPoints>::New();
      this->RotationHandle2Points = vtkSmartPointer<vtkPoints>::New();

      // Handle default position and orientation
      double handleOriginDefault[3] = { ROTATION_HANDLE_DEFAULT_POSITION[0],
                                        ROTATION_HANDLE_DEFAULT_POSITION[1],
                                        ROTATION_HANDLE_DEFAULT_POSITION[2] };
      double handleOrientationDefault[3] = { ROTATION_HANDLE_DEFAULT_ORIENTATION[0],
                                             ROTATION_HANDLE_DEFAULT_ORIENTATION[1],
                                             ROTATION_HANDLE_DEFAULT_ORIENTATION[2] };
      double handleOrientationDefaultInv[3] = { -ROTATION_HANDLE_DEFAULT_ORIENTATION[0],
                                                -ROTATION_HANDLE_DEFAULT_ORIENTATION[1],
                                                -ROTATION_HANDLE_DEFAULT_ORIENTATION[2] };
      double handleOrientationPerpendicular[3] = { ROTATION_HANDLE_DEFAULT_ORIENTATION[1],
                                                   -ROTATION_HANDLE_DEFAULT_ORIENTATION[0],
                                                   ROTATION_HANDLE_DEFAULT_ORIENTATION[2] };

      if (HANDLES_TYPE == Arrows)
        {
        // Define cone size
        double coneAngleRad = (ROTATION_HANDLE_ARROW_TIP_ANGLE * M_PI) / 180.0;
        double coneRadius = 2 * ROTATION_HANDLE_ARROW_RADIUS;
        double coneLength = coneRadius / tan(coneAngleRad);

        // Define arc points
        double arcTipR[3] = { handleOrientationDefault[0], handleOrientationDefault[1], handleOrientationDefault[2] };
        vtkMath::MultiplyScalar(arcTipR, ROTATION_HANDLE_ARROW_LENGTH / 2.0);
        double arcTipL[3] = { handleOrientationDefaultInv[0], handleOrientationDefaultInv[1], handleOrientationDefaultInv[2] };
        vtkMath::MultiplyScalar(arcTipL, ROTATION_HANDLE_ARROW_LENGTH / 2.0);
        double arcCenter[3] = { handleOrientationPerpendicular[0], handleOrientationPerpendicular[1], handleOrientationPerpendicular[2] };
        vtkMath::MultiplyScalar(arcCenter, -ROTATION_HANDLE_ARROW_LENGTH / 3.0);

        // Translate arc to origin
        double arcRadiusVector[3] = { arcTipR[0] - arcCenter[0], arcTipR[1] - arcCenter[1], arcTipR[2] - arcCenter[2] };
        double arcRadius = vtkMath::Norm(arcRadiusVector);
        double arcChordVector[3] = { arcTipR[0] - arcTipL[0], arcTipR[1] - arcTipL[1], arcTipR[2] - arcTipL[2] };
        double arcChord = vtkMath::Norm(arcChordVector);
        double arcSagitta1 = arcRadius + sqrt(pow(arcRadius,2) - pow(arcChord / 2, 2));
        double arcSagitta2 = arcRadius - sqrt(pow(arcRadius, 2) - pow(arcChord / 2, 2));
        double arcSagitta[3] = { handleOrientationPerpendicular[0], handleOrientationPerpendicular[1], handleOrientationPerpendicular[2] };
        if (arcSagitta1 > arcSagitta2) // keep shortest sagitta
          {
          vtkMath::MultiplyScalar(arcSagitta, arcSagitta2);
          }
        else // keep shortest sagitta
          {
          vtkMath::MultiplyScalar(arcSagitta, arcSagitta1);
          }
        arcTipR[0] = arcTipR[0] - arcSagitta[0];
        arcTipR[1] = arcTipR[1] - arcSagitta[1];
        arcTipR[2] = arcTipR[2] - arcSagitta[2];
        arcTipL[0] = arcTipL[0] - arcSagitta[0];
        arcTipL[1] = arcTipL[1] - arcSagitta[1];
        arcTipL[2] = arcTipL[2] - arcSagitta[2];
        arcCenter[0] = arcCenter[0] - arcSagitta[0];
        arcCenter[1] = arcCenter[1] - arcSagitta[1];
        arcCenter[2] = arcCenter[2] - arcSagitta[2];

        // Define intermediate points for interaction
        double arcMidR[3] = { arcTipR[0] / 2.0, arcTipR[1] / 2.0, arcTipR[2] / 2.0 };
        double arcMidL[3] = { arcTipL[0] / 2.0, arcTipL[1] / 2.0, arcTipL[2] / 2.0 };

        // Define arc tangent vectors
        double arcRadiusVectorR[3] = { arcTipR[0] - arcCenter[0], arcTipR[1] - arcCenter[1], arcTipR[2] - arcCenter[2] };
        double arcRadiusVectorL[3] = { arcTipL[0] - arcCenter[0], arcTipL[1] - arcCenter[1], arcTipL[2] - arcCenter[2] };
        double arcTangentVectorR[3] = { -arcRadiusVectorR[1], arcRadiusVectorR[0], 0.0 };
        double arcTangentVectorL[3] = { arcRadiusVectorL[1], -arcRadiusVectorL[0], 0.0 };
        vtkMath::Normalize(arcTangentVectorR);
        vtkMath::Normalize(arcTangentVectorL);

        // Define cone positions to construct arrows
        double coneCenterR[3] = { arcTipR[0] + arcTangentVectorR[0] * (coneLength / 2.0),
                                  arcTipR[1] + arcTangentVectorR[1] * (coneLength / 2.0),
                                  arcTipR[2] + arcTangentVectorR[2] * (coneLength / 2.0) };
        double coneCenterL[3] = { arcTipL[0] + arcTangentVectorL[0] * (coneLength / 2.0),
                                  arcTipL[1] + arcTangentVectorL[1] * (coneLength / 2.0),
                                  arcTipL[2] + arcTangentVectorL[2] * (coneLength / 2.0) };

        // Rotation handle 1
        vtkNew<vtkArcSource> rotationHandle1ArcSource;
        rotationHandle1ArcSource->SetResolution(50);
        rotationHandle1ArcSource->SetPoint1(arcTipR);
        rotationHandle1ArcSource->SetPoint2(arcTipL);
        rotationHandle1ArcSource->SetCenter(arcCenter);
        vtkNew<vtkTubeFilter> rotationHandle1ArcTubeFilter;
        rotationHandle1ArcTubeFilter->SetInputConnection(rotationHandle1ArcSource->GetOutputPort());
        rotationHandle1ArcTubeFilter->SetRadius(ROTATION_HANDLE_ARROW_RADIUS);
        rotationHandle1ArcTubeFilter->SetNumberOfSides(16);
        rotationHandle1ArcTubeFilter->SetCapping(true);
        vtkNew<vtkConeSource> rotationHandle1RightConeSource;
        rotationHandle1RightConeSource->SetResolution(50);
        rotationHandle1RightConeSource->SetRadius(coneRadius);
        rotationHandle1RightConeSource->SetDirection(arcTangentVectorR);
        rotationHandle1RightConeSource->SetHeight(coneLength);
        rotationHandle1RightConeSource->SetCenter(coneCenterR);
        vtkNew<vtkConeSource> rotationHandle1LeftConeSource;
        rotationHandle1LeftConeSource->SetResolution(50);
        rotationHandle1LeftConeSource->SetRadius(coneRadius);
        rotationHandle1LeftConeSource->SetDirection(arcTangentVectorL);
        rotationHandle1LeftConeSource->SetHeight(coneLength);
        rotationHandle1LeftConeSource->SetCenter(coneCenterL);
        this->RotationHandle1 = vtkSmartPointer<vtkAppendPolyData>::New();
        this->RotationHandle1->AddInputConnection(rotationHandle1ArcTubeFilter->GetOutputPort());
        this->RotationHandle1->AddInputConnection(rotationHandle1RightConeSource->GetOutputPort());
        this->RotationHandle1->AddInputConnection(rotationHandle1LeftConeSource->GetOutputPort());
        this->RotationHandle1ToWorldTransform = vtkSmartPointer<vtkTransform>::New();
        this->RotationHandle1TransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
        this->RotationHandle1TransformFilter->SetInputConnection(this->RotationHandle1->GetOutputPort());
        this->RotationHandle1TransformFilter->SetTransform(this->RotationHandle1ToWorldTransform);
        this->RotationHandle1Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
        this->RotationHandle1Property = vtkSmartPointer<vtkProperty2D>::New();
        this->RotationHandle1Actor = vtkSmartPointer<vtkActor2D>::New();
        this->RotationHandle1Actor->SetVisibility(false); // invisible until slice node is set
        this->RotationHandle1Mapper->SetInputConnection(this->RotationHandle1TransformFilter->GetOutputPort());
        this->RotationHandle1Actor->SetMapper(this->RotationHandle1Mapper);
        this->RotationHandle1Actor->SetProperty(this->RotationHandle1Property);

        // Rotation handle 2
        vtkNew<vtkArcSource> rotationHandle2ArcSource;
        rotationHandle2ArcSource->SetResolution(50);
        rotationHandle2ArcSource->SetPoint1(arcTipR);
        rotationHandle2ArcSource->SetPoint2(arcTipL);
        rotationHandle2ArcSource->SetCenter(arcCenter);
        vtkNew<vtkTubeFilter> rotationHandle2ArcTubeFilter;
        rotationHandle2ArcTubeFilter->SetInputConnection(rotationHandle2ArcSource->GetOutputPort());
        rotationHandle2ArcTubeFilter->SetRadius(ROTATION_HANDLE_ARROW_RADIUS);
        rotationHandle2ArcTubeFilter->SetNumberOfSides(16);
        rotationHandle2ArcTubeFilter->SetCapping(true);
        vtkNew<vtkConeSource> rotationHandle2RightConeSource;
        rotationHandle2RightConeSource->SetResolution(50);
        rotationHandle2RightConeSource->SetRadius(coneRadius);
        rotationHandle2RightConeSource->SetDirection(arcTangentVectorR);
        rotationHandle2RightConeSource->SetHeight(coneLength);
        rotationHandle2RightConeSource->SetCenter(coneCenterR);
        vtkNew<vtkConeSource> rotationHandle2LeftConeSource;
        rotationHandle2LeftConeSource->SetResolution(50);
        rotationHandle2LeftConeSource->SetRadius(coneRadius);
        rotationHandle2LeftConeSource->SetDirection(arcTangentVectorL);
        rotationHandle2LeftConeSource->SetHeight(coneLength);
        rotationHandle2LeftConeSource->SetCenter(coneCenterL);
        this->RotationHandle2 = vtkSmartPointer<vtkAppendPolyData>::New();
        this->RotationHandle2->AddInputConnection(rotationHandle2ArcTubeFilter->GetOutputPort());
        this->RotationHandle2->AddInputConnection(rotationHandle2RightConeSource->GetOutputPort());
        this->RotationHandle2->AddInputConnection(rotationHandle2LeftConeSource->GetOutputPort());
        this->RotationHandle2ToWorldTransform = vtkSmartPointer<vtkTransform>::New();
        this->RotationHandle2TransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
        this->RotationHandle2TransformFilter->SetInputConnection(this->RotationHandle2->GetOutputPort());
        this->RotationHandle2TransformFilter->SetTransform(this->RotationHandle2ToWorldTransform);
        this->RotationHandle2Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
        this->RotationHandle2Property = vtkSmartPointer<vtkProperty2D>::New();
        this->RotationHandle2Actor = vtkSmartPointer<vtkActor2D>::New();
        this->RotationHandle2Actor->SetVisibility(false); // invisible until slice node is set
        this->RotationHandle2Mapper->SetInputConnection(this->RotationHandle2TransformFilter->GetOutputPort());
        this->RotationHandle2Actor->SetMapper(this->RotationHandle2Mapper);
        this->RotationHandle2Actor->SetProperty(this->RotationHandle2Property);

        // Handle points
        this->RotationHandle1Points->InsertNextPoint(handleOriginDefault);
        this->RotationHandle1Points->InsertNextPoint(arcTipR);
        this->RotationHandle1Points->InsertNextPoint(arcMidR);
        this->RotationHandle1Points->InsertNextPoint(coneCenterR);
        this->RotationHandle1Points->InsertNextPoint(arcTipL);
        this->RotationHandle1Points->InsertNextPoint(arcMidL);
        this->RotationHandle1Points->InsertNextPoint(coneCenterL);
        this->RotationHandle2Points->InsertNextPoint(handleOriginDefault);
        this->RotationHandle2Points->InsertNextPoint(arcTipR);
        this->RotationHandle2Points->InsertNextPoint(arcMidR);
        this->RotationHandle2Points->InsertNextPoint(coneCenterR);
        this->RotationHandle2Points->InsertNextPoint(arcTipL);
        this->RotationHandle2Points->InsertNextPoint(arcMidL);
        this->RotationHandle2Points->InsertNextPoint(coneCenterL);
        }
      else if (HANDLES_TYPE == Circles)
        {
        // Rotation sphere 1
        vtkNew<vtkSphereSource> rotationHandle1SphereSource;
        rotationHandle1SphereSource->SetRadius(ROTATION_HANDLE_CIRCLE_RADIUS);
        rotationHandle1SphereSource->SetThetaResolution(HANDLES_CIRCLE_THETA_RESOLUTION);
        rotationHandle1SphereSource->SetPhiResolution(HANDLES_CIRCLE_PHI_RESOLUTION);
        rotationHandle1SphereSource->SetCenter(handleOriginDefault);
        rotationHandle1SphereSource->Update();
        this->RotationHandle1 = vtkSmartPointer<vtkAppendPolyData>::New();
        this->RotationHandle1->AddInputConnection(rotationHandle1SphereSource->GetOutputPort());
        this->RotationHandle1ToWorldTransform = vtkSmartPointer<vtkTransform>::New();
        this->RotationHandle1TransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
        this->RotationHandle1TransformFilter->SetInputConnection(this->RotationHandle1->GetOutputPort());
        this->RotationHandle1TransformFilter->SetTransform(this->RotationHandle1ToWorldTransform);
        this->RotationHandle1Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
        this->RotationHandle1Property = vtkSmartPointer<vtkProperty2D>::New();
        this->RotationHandle1Actor = vtkSmartPointer<vtkActor2D>::New();
        this->RotationHandle1Actor->SetVisibility(false); // invisible until slice node is set
        this->RotationHandle1Mapper->SetInputConnection(this->RotationHandle1TransformFilter->GetOutputPort());
        this->RotationHandle1Actor->SetMapper(this->RotationHandle1Mapper);
        this->RotationHandle1Actor->SetProperty(this->RotationHandle1Property);

        // Rotation sphere 2
        vtkNew<vtkSphereSource> rotationHandle2SphereSource;
        rotationHandle2SphereSource->SetRadius(ROTATION_HANDLE_CIRCLE_RADIUS);
        rotationHandle2SphereSource->SetThetaResolution(HANDLES_CIRCLE_THETA_RESOLUTION);
        rotationHandle2SphereSource->SetPhiResolution(HANDLES_CIRCLE_PHI_RESOLUTION);
        rotationHandle2SphereSource->SetCenter(handleOriginDefault);
        rotationHandle2SphereSource->Update();
        this->RotationHandle2 = vtkSmartPointer<vtkAppendPolyData>::New();
        this->RotationHandle2->AddInputConnection(rotationHandle2SphereSource->GetOutputPort());
        this->RotationHandle2ToWorldTransform = vtkSmartPointer<vtkTransform>::New();
        this->RotationHandle2TransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
        this->RotationHandle2TransformFilter->SetInputConnection(this->RotationHandle2->GetOutputPort());
        this->RotationHandle2TransformFilter->SetTransform(this->RotationHandle2ToWorldTransform);
        this->RotationHandle2Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
        this->RotationHandle2Property = vtkSmartPointer<vtkProperty2D>::New();
        this->RotationHandle2Actor = vtkSmartPointer<vtkActor2D>::New();
        this->RotationHandle2Actor->SetVisibility(false); // invisible until slice node is set
        this->RotationHandle2Mapper->SetInputConnection(this->RotationHandle2TransformFilter->GetOutputPort());
        this->RotationHandle2Actor->SetMapper(this->RotationHandle2Mapper);
        this->RotationHandle2Actor->SetProperty(this->RotationHandle2Property);

        // Handle points
        this->RotationHandle1Points->InsertNextPoint(handleOriginDefault);
        this->RotationHandle2Points->InsertNextPoint(handleOriginDefault);
        }
      }

    //----------------------------------------------------------------------
    void CreateSliceOffsetHandles()
    {
      // Create list of points to store position of handles
      this->SliceOffsetHandle1Points = vtkSmartPointer<vtkPoints>::New();
      this->SliceOffsetHandle2Points = vtkSmartPointer<vtkPoints>::New();

      // Handle default position and orientation
      double handleOriginDefault[3] = { SLICEOFSSET_HANDLE_DEFAULT_POSITION[0],
                                        SLICEOFSSET_HANDLE_DEFAULT_POSITION[1],
                                        SLICEOFSSET_HANDLE_DEFAULT_POSITION[2] };
      double handleOrientationDefault[3] = { SLICEOFSSET_HANDLE_DEFAULT_ORIENTATION[0],
                                             SLICEOFSSET_HANDLE_DEFAULT_ORIENTATION[1],
                                             SLICEOFSSET_HANDLE_DEFAULT_ORIENTATION[2] };
      double handleOrientationDefaultInv[3] = { -SLICEOFSSET_HANDLE_DEFAULT_ORIENTATION[0],
                                                -SLICEOFSSET_HANDLE_DEFAULT_ORIENTATION[1],
                                                -SLICEOFSSET_HANDLE_DEFAULT_ORIENTATION[2] };

      if (HANDLES_TYPE == Arrows)
        {
        // Define cone size
        double coneAngleRad = (SLICEOFSSET_HANDLE_ARROW_TIP_ANGLE * M_PI) / 180.0;
        double coneRadius = 2 * SLICEOFSSET_HANDLE_ARROW_RADIUS;
        double coneLength = coneRadius / tan(coneAngleRad);

        // Define cylinder size
        double cylinderLength = SLICEOFSSET_HANDLE_ARROW_LENGTH - coneLength * 2;
        double cylinderRadius = SLICEOFSSET_HANDLE_ARROW_RADIUS;

        // Define cone positions to construct arrows
        double coneCenterR[3] = { handleOrientationDefault[0], handleOrientationDefault[1], handleOrientationDefault[2] };
        vtkMath::MultiplyScalar(coneCenterR, cylinderLength / 2 + coneLength / 2);
        double coneTipR[3] = { handleOrientationDefault[0], handleOrientationDefault[1], handleOrientationDefault[2] };
        vtkMath::MultiplyScalar(coneTipR, cylinderLength / 2 + coneLength);
        double coneBaseR[3] = { handleOrientationDefault[0], handleOrientationDefault[1], handleOrientationDefault[2] };
        vtkMath::MultiplyScalar(coneBaseR, cylinderLength / 2);
        double coneCenterL[3] = { handleOrientationDefaultInv[0], handleOrientationDefaultInv[1], handleOrientationDefaultInv[2] };
        vtkMath::MultiplyScalar(coneCenterL, cylinderLength / 2 + coneLength / 2);
        double coneTipL[3] = { handleOrientationDefaultInv[0], handleOrientationDefaultInv[1], handleOrientationDefaultInv[2] };
        vtkMath::MultiplyScalar(coneTipL, cylinderLength / 2 + coneLength);
        double coneBaseL[3] = { handleOrientationDefaultInv[0], handleOrientationDefaultInv[1], handleOrientationDefaultInv[2] };
        vtkMath::MultiplyScalar(coneBaseL, cylinderLength / 2);

        // Translation handle 1
        vtkNew<vtkLineSource> sliceOffsetHandle1LineSource;
        sliceOffsetHandle1LineSource->SetResolution(50);
        sliceOffsetHandle1LineSource->SetPoint1(coneBaseR);
        sliceOffsetHandle1LineSource->SetPoint2(coneBaseL);
        vtkNew<vtkTubeFilter> sliceOffsetHandle1LineTubeFilter;
        sliceOffsetHandle1LineTubeFilter->SetInputConnection(sliceOffsetHandle1LineSource->GetOutputPort());
        sliceOffsetHandle1LineTubeFilter->SetRadius(SLICEOFSSET_HANDLE_ARROW_RADIUS);
        sliceOffsetHandle1LineTubeFilter->SetNumberOfSides(16);
        sliceOffsetHandle1LineTubeFilter->SetCapping(true);
        vtkNew<vtkConeSource> sliceOffsetHandle1RightConeSource;
        sliceOffsetHandle1RightConeSource->SetResolution(50);
        sliceOffsetHandle1RightConeSource->SetRadius(coneRadius);
        sliceOffsetHandle1RightConeSource->SetDirection(handleOrientationDefault);
        sliceOffsetHandle1RightConeSource->SetHeight(coneLength);
        sliceOffsetHandle1RightConeSource->SetCenter(coneCenterR);
        vtkNew<vtkConeSource> sliceOffsetHandle1LeftConeSource;
        sliceOffsetHandle1LeftConeSource->SetResolution(50);
        sliceOffsetHandle1LeftConeSource->SetRadius(coneRadius);
        sliceOffsetHandle1LeftConeSource->SetDirection(handleOrientationDefaultInv);
        sliceOffsetHandle1LeftConeSource->SetHeight(coneLength);
        sliceOffsetHandle1LeftConeSource->SetCenter(coneCenterL);
        this->SliceOffsetHandle1 = vtkSmartPointer<vtkAppendPolyData>::New();
        this->SliceOffsetHandle1->AddInputConnection(sliceOffsetHandle1LineTubeFilter->GetOutputPort());
        this->SliceOffsetHandle1->AddInputConnection(sliceOffsetHandle1RightConeSource->GetOutputPort());
        this->SliceOffsetHandle1->AddInputConnection(sliceOffsetHandle1LeftConeSource->GetOutputPort());
        this->SliceOffsetHandle1ToWorldTransform = vtkSmartPointer<vtkTransform>::New();
        this->SliceOffsetHandle1TransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
        this->SliceOffsetHandle1TransformFilter->SetInputConnection(this->SliceOffsetHandle1->GetOutputPort());
        this->SliceOffsetHandle1TransformFilter->SetTransform(this->SliceOffsetHandle1ToWorldTransform);
        this->SliceOffsetHandle1Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
        this->SliceOffsetHandle1Property = vtkSmartPointer<vtkProperty2D>::New();
        this->SliceOffsetHandle1Actor = vtkSmartPointer<vtkActor2D>::New();
        this->SliceOffsetHandle1Actor->SetVisibility(false); // invisible until slice node is set
        this->SliceOffsetHandle1Mapper->SetInputConnection(this->SliceOffsetHandle1TransformFilter->GetOutputPort());
        this->SliceOffsetHandle1Actor->SetMapper(this->SliceOffsetHandle1Mapper);
        this->SliceOffsetHandle1Actor->SetProperty(this->SliceOffsetHandle1Property);

        // Translation handle 2
        vtkNew<vtkLineSource> sliceOffsetHandle2LineSource;
        sliceOffsetHandle2LineSource->SetResolution(50);
        sliceOffsetHandle2LineSource->SetPoint1(coneBaseR);
        sliceOffsetHandle2LineSource->SetPoint2(coneBaseL);
        vtkNew<vtkTubeFilter> sliceOffsetHandle2LineTubeFilter;
        sliceOffsetHandle2LineTubeFilter->SetInputConnection(sliceOffsetHandle2LineSource->GetOutputPort());
        sliceOffsetHandle2LineTubeFilter->SetRadius(SLICEOFSSET_HANDLE_ARROW_RADIUS);
        sliceOffsetHandle2LineTubeFilter->SetNumberOfSides(16);
        sliceOffsetHandle2LineTubeFilter->SetCapping(true);
        vtkNew<vtkConeSource> sliceOffsetHandle2RightConeSource;
        sliceOffsetHandle2RightConeSource->SetResolution(50);
        sliceOffsetHandle2RightConeSource->SetRadius(coneRadius);
        sliceOffsetHandle2RightConeSource->SetDirection(handleOrientationDefault);
        sliceOffsetHandle2RightConeSource->SetHeight(coneLength);
        sliceOffsetHandle2RightConeSource->SetCenter(coneCenterR);
        vtkNew<vtkConeSource> sliceOffsetHandle2LeftConeSource;
        sliceOffsetHandle2LeftConeSource->SetResolution(50);
        sliceOffsetHandle2LeftConeSource->SetRadius(coneRadius);
        sliceOffsetHandle2LeftConeSource->SetDirection(handleOrientationDefaultInv);
        sliceOffsetHandle2LeftConeSource->SetHeight(coneLength);
        sliceOffsetHandle2LeftConeSource->SetCenter(coneCenterL);
        this->SliceOffsetHandle2 = vtkSmartPointer<vtkAppendPolyData>::New();
        this->SliceOffsetHandle2->AddInputConnection(sliceOffsetHandle2LineTubeFilter->GetOutputPort());
        this->SliceOffsetHandle2->AddInputConnection(sliceOffsetHandle2RightConeSource->GetOutputPort());
        this->SliceOffsetHandle2->AddInputConnection(sliceOffsetHandle2LeftConeSource->GetOutputPort());
        this->SliceOffsetHandle2ToWorldTransform = vtkSmartPointer<vtkTransform>::New();
        this->SliceOffsetHandle2TransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
        this->SliceOffsetHandle2TransformFilter->SetInputConnection(this->SliceOffsetHandle2->GetOutputPort());
        this->SliceOffsetHandle2TransformFilter->SetTransform(this->SliceOffsetHandle2ToWorldTransform);
        this->SliceOffsetHandle2Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
        this->SliceOffsetHandle2Property = vtkSmartPointer<vtkProperty2D>::New();
        this->SliceOffsetHandle2Actor = vtkSmartPointer<vtkActor2D>::New();
        this->SliceOffsetHandle2Actor->SetVisibility(false); // invisible until slice node is set
        this->SliceOffsetHandle2Mapper->SetInputConnection(this->SliceOffsetHandle2TransformFilter->GetOutputPort());
        this->SliceOffsetHandle2Actor->SetMapper(this->SliceOffsetHandle2Mapper);
        this->SliceOffsetHandle2Actor->SetProperty(this->SliceOffsetHandle2Property);

        // Handle points
        this->SliceOffsetHandle1Points->InsertNextPoint(handleOriginDefault);
        this->SliceOffsetHandle1Points->InsertNextPoint(coneTipR);
        this->SliceOffsetHandle1Points->InsertNextPoint(coneCenterR);
        this->SliceOffsetHandle1Points->InsertNextPoint(coneBaseR);
        this->SliceOffsetHandle1Points->InsertNextPoint(coneTipL);
        this->SliceOffsetHandle1Points->InsertNextPoint(coneCenterL);
        this->SliceOffsetHandle1Points->InsertNextPoint(coneBaseL);
        this->SliceOffsetHandle2Points->InsertNextPoint(handleOriginDefault);
        this->SliceOffsetHandle2Points->InsertNextPoint(coneTipR);
        this->SliceOffsetHandle2Points->InsertNextPoint(coneCenterR);
        this->SliceOffsetHandle2Points->InsertNextPoint(coneBaseR);
        this->SliceOffsetHandle2Points->InsertNextPoint(coneTipL);
        this->SliceOffsetHandle2Points->InsertNextPoint(coneCenterL);
        this->SliceOffsetHandle2Points->InsertNextPoint(coneBaseL);
        }
      else if (HANDLES_TYPE == Circles)
        {
        // Translation sphere 1
        vtkNew<vtkSphereSource> sliceOffsetHandle1SphereSource;
        sliceOffsetHandle1SphereSource->SetRadius(SLICEOFSSET_HANDLE_CIRCLE_RADIUS);
        sliceOffsetHandle1SphereSource->SetThetaResolution(HANDLES_CIRCLE_THETA_RESOLUTION);
        sliceOffsetHandle1SphereSource->SetPhiResolution(HANDLES_CIRCLE_PHI_RESOLUTION);
        sliceOffsetHandle1SphereSource->SetCenter(handleOriginDefault);
        sliceOffsetHandle1SphereSource->Update();
        this->SliceOffsetHandle1 = vtkSmartPointer<vtkAppendPolyData>::New();
        this->SliceOffsetHandle1->AddInputConnection(sliceOffsetHandle1SphereSource->GetOutputPort());
        this->SliceOffsetHandle1ToWorldTransform = vtkSmartPointer<vtkTransform>::New();
        this->SliceOffsetHandle1TransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
        this->SliceOffsetHandle1TransformFilter->SetInputConnection(this->SliceOffsetHandle1->GetOutputPort());
        this->SliceOffsetHandle1TransformFilter->SetTransform(this->SliceOffsetHandle1ToWorldTransform);
        this->SliceOffsetHandle1Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
        this->SliceOffsetHandle1Property = vtkSmartPointer<vtkProperty2D>::New();
        this->SliceOffsetHandle1Actor = vtkSmartPointer<vtkActor2D>::New();
        this->SliceOffsetHandle1Actor->SetVisibility(false); // invisible until slice node is set
        this->SliceOffsetHandle1Mapper->SetInputConnection(this->SliceOffsetHandle1TransformFilter->GetOutputPort());
        this->SliceOffsetHandle1Actor->SetMapper(this->SliceOffsetHandle1Mapper);
        this->SliceOffsetHandle1Actor->SetProperty(this->SliceOffsetHandle1Property);

        // Translation sphere 2
        vtkNew<vtkSphereSource> sliceOffsetHandle2SphereSource;
        sliceOffsetHandle2SphereSource->SetRadius(SLICEOFSSET_HANDLE_CIRCLE_RADIUS);
        sliceOffsetHandle2SphereSource->SetThetaResolution(HANDLES_CIRCLE_THETA_RESOLUTION);
        sliceOffsetHandle2SphereSource->SetPhiResolution(HANDLES_CIRCLE_PHI_RESOLUTION);
        sliceOffsetHandle2SphereSource->SetCenter(handleOriginDefault);
        sliceOffsetHandle2SphereSource->Update();
        this->SliceOffsetHandle2 = vtkSmartPointer<vtkAppendPolyData>::New();
        this->SliceOffsetHandle2->AddInputConnection(sliceOffsetHandle2SphereSource->GetOutputPort());
        this->SliceOffsetHandle2ToWorldTransform = vtkSmartPointer<vtkTransform>::New();
        this->SliceOffsetHandle2TransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
        this->SliceOffsetHandle2TransformFilter->SetInputConnection(this->SliceOffsetHandle2->GetOutputPort());
        this->SliceOffsetHandle2TransformFilter->SetTransform(this->SliceOffsetHandle2ToWorldTransform);
        this->SliceOffsetHandle2Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
        this->SliceOffsetHandle2Property = vtkSmartPointer<vtkProperty2D>::New();
        this->SliceOffsetHandle2Actor = vtkSmartPointer<vtkActor2D>::New();
        this->SliceOffsetHandle2Actor->SetVisibility(false); // invisible until slice node is set
        this->SliceOffsetHandle2Mapper->SetInputConnection(this->SliceOffsetHandle2TransformFilter->GetOutputPort());
        this->SliceOffsetHandle2Actor->SetMapper(this->SliceOffsetHandle2Mapper);
        this->SliceOffsetHandle2Actor->SetProperty(this->SliceOffsetHandle2Property);

        // Handle points
        this->SliceOffsetHandle1Points->InsertNextPoint(handleOriginDefault);
        this->SliceOffsetHandle2Points->InsertNextPoint(handleOriginDefault);
        }
    }

    //----------------------------------------------------------------------
    void SetAndObserveSliceLogic(vtkMRMLSliceLogic* sliceLogic, vtkCallbackCommand* callback)
    {

      if (sliceLogic != this->SliceLogic || callback != this->Callback)
        {
        if (this->SliceLogic && this->Callback)
          {
          this->SliceLogic->RemoveObserver(this->Callback);
          }
        if (sliceLogic)
          {
          sliceLogic->AddObserver(vtkCommand::ModifiedEvent, callback);
          sliceLogic->AddObserver(vtkMRMLSliceLogic::CompositeModifiedEvent, callback);
          }
        this->SliceLogic = sliceLogic;
        }
      this->Callback = callback;
    }

    //----------------------------------------------------------------------
    void GetActors2D(vtkPropCollection* pc)
    {
      pc->AddItem(this->IntersectionLine1Actor);
      pc->AddItem(this->IntersectionLine2Actor);
      pc->AddItem(this->TranslationOuterHandleActor);
      pc->AddItem(this->TranslationInnerHandleActor);
      pc->AddItem(this->RotationHandle1Actor);
      pc->AddItem(this->RotationHandle2Actor);
      pc->AddItem(this->SliceOffsetHandle1Actor);
      pc->AddItem(this->SliceOffsetHandle2Actor);
    }

    //----------------------------------------------------------------------
    void AddActors(vtkRenderer* renderer)
    {
      if (!renderer)
        {
        return;
        }
      renderer->AddViewProp(this->IntersectionLine1Actor);
      renderer->AddViewProp(this->IntersectionLine2Actor);
      renderer->AddViewProp(this->TranslationOuterHandleActor);
      renderer->AddViewProp(this->TranslationInnerHandleActor);
      renderer->AddViewProp(this->RotationHandle1Actor);
      renderer->AddViewProp(this->RotationHandle2Actor);
      renderer->AddViewProp(this->SliceOffsetHandle1Actor);
      renderer->AddViewProp(this->SliceOffsetHandle2Actor);
    }

    //----------------------------------------------------------------------
    void ReleaseGraphicsResources(vtkWindow* win)
    {
      this->IntersectionLine1Actor->ReleaseGraphicsResources(win);
      this->IntersectionLine2Actor->ReleaseGraphicsResources(win);
      this->TranslationOuterHandleActor->ReleaseGraphicsResources(win);
      this->TranslationInnerHandleActor->ReleaseGraphicsResources(win);
      this->RotationHandle1Actor->ReleaseGraphicsResources(win);
      this->RotationHandle2Actor->ReleaseGraphicsResources(win);
      this->SliceOffsetHandle1Actor->ReleaseGraphicsResources(win);
      this->SliceOffsetHandle2Actor->ReleaseGraphicsResources(win);
    }

    //----------------------------------------------------------------------
    int RenderOverlay(vtkViewport* viewport)
    {
      int count = 0;
      if (this->TranslationInnerHandleActor->GetVisibility())
        {
        count += this->TranslationInnerHandleActor->RenderOverlay(viewport);
        }
      return count;
    }

    //----------------------------------------------------------------------
    void RemoveActors(vtkRenderer* renderer)
    {
      if (!renderer)
        {
        return;
        }
      renderer->RemoveViewProp(this->IntersectionLine1Actor);
      renderer->RemoveViewProp(this->IntersectionLine2Actor);
      renderer->RemoveViewProp(this->TranslationOuterHandleActor);
      renderer->RemoveViewProp(this->TranslationInnerHandleActor);
      renderer->RemoveViewProp(this->RotationHandle1Actor);
      renderer->RemoveViewProp(this->RotationHandle2Actor);
      renderer->RemoveViewProp(this->SliceOffsetHandle1Actor);
      renderer->RemoveViewProp(this->SliceOffsetHandle2Actor);
    }

    //----------------------------------------------------------------------
    void SetVisibility(bool visibility)
      {
      this->IntersectionLine1Actor->SetVisibility(visibility);
      this->IntersectionLine2Actor->SetVisibility(visibility);
      if (HANDLES_ALWAYS_VISIBLE)
        {
        this->TranslationOuterHandleActor->SetVisibility(visibility);
        this->TranslationInnerHandleActor->SetVisibility(visibility);
        this->RotationHandle1Actor->SetVisibility(visibility);
        this->RotationHandle2Actor->SetVisibility(visibility);
        this->SliceOffsetHandle1Actor->SetVisibility(visibility);
        this->SliceOffsetHandle2Actor->SetVisibility(visibility);
        }
      }

    //----------------------------------------------------------------------
    void SetHandlesVisibility(bool visibility)
      {
      if (VISUALIZATION_MODE == ShowIntersection)
        {
        this->TranslationOuterHandleActor->SetVisibility(visibility);
        this->TranslationInnerHandleActor->SetVisibility(visibility);
        }
      this->RotationHandle1Actor->SetVisibility(visibility);
      this->RotationHandle2Actor->SetVisibility(visibility);
      this->SliceOffsetHandle1Actor->SetVisibility(visibility);
      this->SliceOffsetHandle2Actor->SetVisibility(visibility);
      }

    //----------------------------------------------------------------------
    void SetHandlesOpacity(double opacity)
      {
      this->TranslationOuterHandleProperty->SetOpacity(opacity);
      this->TranslationInnerHandleProperty->SetOpacity(opacity);
      this->RotationHandle1Property->SetOpacity(opacity);
      this->RotationHandle2Property->SetOpacity(opacity);
      this->SliceOffsetHandle1Property->SetOpacity(opacity);
      this->SliceOffsetHandle2Property->SetOpacity(opacity);
      }

    //----------------------------------------------------------------------
    bool GetVisibility()
    {
      return this->IntersectionLine1Actor->GetVisibility();
    }

    vtkSmartPointer<vtkLineSource> IntersectionLine1;
    vtkSmartPointer<vtkPolyDataMapper2D> IntersectionLine1Mapper;
    vtkSmartPointer<vtkProperty2D> IntersectionLine1Property;
    vtkSmartPointer<vtkActor2D> IntersectionLine1Actor;

    vtkSmartPointer<vtkLineSource> IntersectionLine2;
    vtkSmartPointer<vtkPolyDataMapper2D> IntersectionLine2Mapper;
    vtkSmartPointer<vtkProperty2D> IntersectionLine2Property;
    vtkSmartPointer<vtkActor2D> IntersectionLine2Actor;

    vtkSmartPointer<vtkSphereSource> TranslationOuterHandle;
    vtkSmartPointer<vtkPolyDataMapper2D> TranslationOuterHandleMapper;
    vtkSmartPointer<vtkProperty2D> TranslationOuterHandleProperty;
    vtkSmartPointer<vtkActor2D> TranslationOuterHandleActor;
    vtkSmartPointer<vtkSphereSource> TranslationInnerHandle;
    vtkSmartPointer<vtkPolyDataMapper2D> TranslationInnerHandleMapper;
    vtkSmartPointer<vtkProperty2D> TranslationInnerHandleProperty;
    vtkSmartPointer<vtkActor2D> TranslationInnerHandleActor;

    vtkSmartPointer<vtkAppendPolyData> RotationHandle1;
    vtkSmartPointer<vtkTransform> RotationHandle1ToWorldTransform;
    vtkSmartPointer<vtkTransformPolyDataFilter> RotationHandle1TransformFilter;
    vtkSmartPointer<vtkPolyDataMapper2D> RotationHandle1Mapper;
    vtkSmartPointer<vtkProperty2D> RotationHandle1Property;
    vtkSmartPointer<vtkActor2D> RotationHandle1Actor;

    vtkSmartPointer<vtkAppendPolyData> RotationHandle2;
    vtkSmartPointer<vtkTransform> RotationHandle2ToWorldTransform;
    vtkSmartPointer<vtkTransformPolyDataFilter> RotationHandle2TransformFilter;
    vtkSmartPointer<vtkPolyDataMapper2D> RotationHandle2Mapper;
    vtkSmartPointer<vtkProperty2D> RotationHandle2Property;
    vtkSmartPointer<vtkActor2D> RotationHandle2Actor;

    vtkSmartPointer<vtkAppendPolyData> SliceOffsetHandle1;
    vtkSmartPointer<vtkTransform> SliceOffsetHandle1ToWorldTransform;
    vtkSmartPointer<vtkTransformPolyDataFilter> SliceOffsetHandle1TransformFilter;
    vtkSmartPointer<vtkPolyDataMapper2D> SliceOffsetHandle1Mapper;
    vtkSmartPointer<vtkProperty2D> SliceOffsetHandle1Property;
    vtkSmartPointer<vtkActor2D> SliceOffsetHandle1Actor;

    vtkSmartPointer<vtkAppendPolyData> SliceOffsetHandle2;
    vtkSmartPointer<vtkTransform> SliceOffsetHandle2ToWorldTransform;
    vtkSmartPointer<vtkTransformPolyDataFilter> SliceOffsetHandle2TransformFilter;
    vtkSmartPointer<vtkPolyDataMapper2D> SliceOffsetHandle2Mapper;
    vtkSmartPointer<vtkProperty2D> SliceOffsetHandle2Property;
    vtkSmartPointer<vtkActor2D> SliceOffsetHandle2Actor;

    vtkWeakPointer<vtkMRMLSliceLogic> SliceLogic;
    vtkWeakPointer<vtkCallbackCommand> Callback;

    vtkSmartPointer<vtkPolyData> IntersectionLine1Points;
    vtkSmartPointer<vtkPolyData> IntersectionLine2Points;
    vtkSmartPointer<vtkPolyData> RotationHandlePoints;
    vtkSmartPointer<vtkPolyData> TranslationHandlePoints;
    vtkSmartPointer<vtkPolyData> SliceOffsetHandlePoints;

    vtkSmartPointer<vtkPoints> SliceOffsetHandle1Points;
    vtkSmartPointer<vtkPoints> SliceOffsetHandle2Points;
    vtkSmartPointer<vtkPoints> RotationHandle1Points;
    vtkSmartPointer<vtkPoints> RotationHandle2Points;
  };

class vtkMRMLSliceIntersectionInteractionRepresentation::vtkInternal
{
  public:
    vtkInternal(vtkMRMLSliceIntersectionInteractionRepresentation* external);
    ~vtkInternal();

    vtkMRMLSliceIntersectionInteractionRepresentation* External;

    vtkSmartPointer<vtkMRMLSliceNode> SliceNode;

    std::deque<SliceIntersectionInteractionDisplayPipeline*> SliceIntersectionInteractionDisplayPipelines;
    vtkNew<vtkCallbackCommand> SliceNodeModifiedCommand;
};

//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::vtkInternal
::vtkInternal(vtkMRMLSliceIntersectionInteractionRepresentation* external)
{
  this->External = external;
}

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::vtkInternal::~vtkInternal() = default;

//----------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::vtkMRMLSliceIntersectionInteractionRepresentation()
{
  this->MRMLApplicationLogic = nullptr;

  this->Internal = new vtkInternal(this);
  this->Internal->SliceNodeModifiedCommand->SetClientData(this);
  this->Internal->SliceNodeModifiedCommand->SetCallback(vtkMRMLSliceIntersectionInteractionRepresentation::SliceNodeModifiedCallback);

  this->SliceIntersectionPoint[0] = 0.0;
  this->SliceIntersectionPoint[1] = 0.0;
  this->SliceIntersectionPoint[2] = 0.0;
  this->SliceIntersectionPoint[3] = 1.0; // to allow easy homogeneous transformations

  // Set interaction size. Determines the maximum distance for interaction.
  this->InteractionSize = INTERACTION_SIZE_PIXELS;
}

//----------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::~vtkMRMLSliceIntersectionInteractionRepresentation()
{
  this->SetSliceNode(nullptr);
  this->SetMRMLApplicationLogic(nullptr);
  delete this->Internal;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::GetActors2D(vtkPropCollection * pc)
{
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    (*sliceIntersectionIt)->GetActors2D(pc);
    }
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::ReleaseGraphicsResources(vtkWindow * win)
{
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    (*sliceIntersectionIt)->ReleaseGraphicsResources(win);
    }
}

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::RenderOverlay(vtkViewport * viewport)
{
  int count = 0;

  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    count += (*sliceIntersectionIt)->RenderOverlay(viewport);
    }
  return count;
}


//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::PrintSelf(ostream & os, vtkIndent indent)
{
  //Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
  this->Superclass::PrintSelf(os, indent);
}


//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SliceNodeModifiedCallback(
  vtkObject * caller, unsigned long vtkNotUsed(eid), void* clientData, void* vtkNotUsed(callData))
{
  vtkMRMLSliceIntersectionInteractionRepresentation* self = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast((vtkObject*)clientData);

  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(caller);
  if (sliceNode)
    {
    self->SliceNodeModified(sliceNode);
    return;
    }

  vtkMRMLSliceLogic* sliceLogic = vtkMRMLSliceLogic::SafeDownCast(caller);
  if (sliceLogic)
    {
    self->UpdateSliceIntersectionDisplay(self->GetDisplayPipelineFromSliceLogic(sliceLogic));
    return;
    }
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SliceNodeModified(vtkMRMLSliceNode * sliceNode)
{
  if (!sliceNode)
    {
    return;
    }
  if (sliceNode == this->Internal->SliceNode)
    {
    // update all slice intersection
    for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
      sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
      sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
      {
      this->UpdateSliceIntersectionDisplay(*sliceIntersectionIt);
      }
    }
}

//----------------------------------------------------------------------
SliceIntersectionInteractionDisplayPipeline* vtkMRMLSliceIntersectionInteractionRepresentation::GetDisplayPipelineFromSliceLogic(vtkMRMLSliceLogic * sliceLogic)
{
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    if (!(*sliceIntersectionIt)->SliceLogic)
      {
      continue;
      }
    if (sliceLogic == (*sliceIntersectionIt)->SliceLogic)
      {
      // found it
      return *sliceIntersectionIt;
      }
    }
  return nullptr;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::UpdateSliceIntersectionDisplay(SliceIntersectionInteractionDisplayPipeline * pipeline)
{
  if (!pipeline || !this->Internal->SliceNode || pipeline->SliceLogic == nullptr)
    {
    return;
    }
  vtkMRMLSliceNode* intersectingSliceNode = pipeline->SliceLogic->GetSliceNode();
  if (!pipeline->SliceLogic || !this->GetVisibility()
    || !intersectingSliceNode
    || this->Internal->SliceNode->GetViewGroup() != intersectingSliceNode->GetViewGroup()
    || !intersectingSliceNode->IsMappedInLayout())
    {
    pipeline->SetVisibility(false);
    pipeline->SetHandlesVisibility(false);
    return;
    }

  vtkMRMLModelDisplayNode* displayNode = nullptr;
  vtkMRMLSliceCompositeNode* compositeNode = nullptr;
  vtkMRMLSliceLogic* sliceLogic = nullptr;
  vtkMRMLApplicationLogic* mrmlAppLogic = this->GetMRMLApplicationLogic();
  if (mrmlAppLogic)
    {
    sliceLogic = mrmlAppLogic->GetSliceLogic(intersectingSliceNode);
    }
  if (sliceLogic)
    {
    displayNode = sliceLogic->GetSliceModelDisplayNode();
    compositeNode = sliceLogic->GetSliceCompositeNode();
    }
  if (displayNode && compositeNode)
    {
    //bool sliceIntersectionVisible = displayNode->GetSliceIntersectionVisibility();
    bool sliceInteractionHandlesVisible = compositeNode->GetSliceIntersectionHandlesVisibility();
    if (!sliceInteractionHandlesVisible)
      {
      pipeline->SetVisibility(false);
      pipeline->SetHandlesVisibility(false);
      return;
      }
    pipeline->IntersectionLine1Property->SetLineWidth(displayNode->GetLineWidth() + INTERSECTION_LINE_EXTRA_THICKNESS);
    pipeline->IntersectionLine2Property->SetLineWidth(displayNode->GetLineWidth() + INTERSECTION_LINE_EXTRA_THICKNESS);
    }

  // Set color of handles
  pipeline->IntersectionLine1Property->SetColor(intersectingSliceNode->GetLayoutColor());
  pipeline->IntersectionLine2Property->SetColor(intersectingSliceNode->GetLayoutColor());
  pipeline->TranslationOuterHandleProperty->SetColor(0, 0, 0); // black color
  pipeline->TranslationInnerHandleProperty->SetColor(255, 255, 255); // white color
  pipeline->RotationHandle1Property->SetColor(intersectingSliceNode->GetLayoutColor());
  pipeline->RotationHandle2Property->SetColor(intersectingSliceNode->GetLayoutColor());
  pipeline->SliceOffsetHandle1Property->SetColor(intersectingSliceNode->GetLayoutColor());
  pipeline->SliceOffsetHandle2Property->SetColor(intersectingSliceNode->GetLayoutColor());

  // Get slice node transforms
  vtkMatrix4x4* xyToRAS = this->Internal->SliceNode->GetXYToRAS();
  vtkNew<vtkMatrix4x4> rasToXY;
  vtkMatrix4x4::Invert(xyToRAS, rasToXY);

  // Get slice intersection point XY
  this->GetSliceIntersectionPoint(); // Get slice intersection point RAS
  double sliceIntersectionPoint_XY[4] = { this->SliceIntersectionPoint[0], this->SliceIntersectionPoint[1], this->SliceIntersectionPoint[2], 1 };
  rasToXY->MultiplyPoint(sliceIntersectionPoint_XY, sliceIntersectionPoint_XY); // Get slice intersection point XY
  double sliceIntersectionPoint_XY_2D[2] = { sliceIntersectionPoint_XY[0], sliceIntersectionPoint_XY[1] };
  double sliceIntersectionPoint_XY_3D[3] = { sliceIntersectionPoint_XY[0], sliceIntersectionPoint_XY[1], 0.0 };

  // Get outer intersection line tips
  vtkMatrix4x4* intersectingXYToRAS = intersectingSliceNode->GetXYToRAS();
  vtkNew<vtkMatrix4x4> intersectingXYToXY;
  vtkMatrix4x4::Multiply4x4(rasToXY, intersectingXYToRAS, intersectingXYToXY);
  double intersectionLineTip1[3] = { 0.0, 0.0, 0.0};
  double intersectionLineTip2[3] = { 0.0, 0.0, 0.0};
  int intersectionFound = this->GetLineTipsFromIntersectingSliceNode(intersectingSliceNode, intersectingXYToXY,
      intersectionLineTip1, intersectionLineTip2);
  double intersectionLineTip1_2D[2] = { intersectionLineTip1[0], intersectionLineTip1[1] };
  double intersectionLineTip2_2D[2] = { intersectionLineTip2[0], intersectionLineTip2[1] };

  // Pipelines not visible if no intersection is found
  if (!intersectionFound)
    {
    pipeline->SetVisibility(false);
    pipeline->SetHandlesVisibility(false);
    return;
    }

  // Get current slice view bounds
  vtkMRMLSliceNode* currentSliceNode = this->GetSliceNode();
  double sliceViewBounds[4] = {};
  this->GetSliceViewBoundariesXY(currentSliceNode, sliceViewBounds);

  // Add margin to slice view bounds
  sliceViewBounds[0] = sliceViewBounds[0] + (sliceViewBounds[1] - sliceViewBounds[0]) * FOV_HANDLES_MARGIN;
  sliceViewBounds[1] = sliceViewBounds[1] - (sliceViewBounds[1] - sliceViewBounds[0]) * FOV_HANDLES_MARGIN;
  sliceViewBounds[2] = sliceViewBounds[2] + (sliceViewBounds[3] - sliceViewBounds[2]) * FOV_HANDLES_MARGIN;
  sliceViewBounds[3] = sliceViewBounds[3] - (sliceViewBounds[3] - sliceViewBounds[2]) * FOV_HANDLES_MARGIN;

  // Adjust intersection points to FOV margins
  bool intersectionFoundLineTip1 = false;
  double intersectionPointLineTip1[2] = { 0.0,0.0 };
  bool intersectionFoundLineTip2 = false;
  double intersectionPointLineTip2[2] = { 0.0,0.0 };
  if ((sliceIntersectionPoint_XY[0] > sliceViewBounds[0]) && // If intersection point is within FOV
    (sliceIntersectionPoint_XY[0] < sliceViewBounds[1]) &&
    (sliceIntersectionPoint_XY[1] > sliceViewBounds[2]) &&
    (sliceIntersectionPoint_XY[1] < sliceViewBounds[3]))
    {
    if ((intersectionLineTip1[0] < sliceViewBounds[0]) || // If line tip 1 is outside the FOV
    (intersectionLineTip1[0] > sliceViewBounds[1]) ||
    (intersectionLineTip1[1] < sliceViewBounds[2]) ||
    (intersectionLineTip1[1] > sliceViewBounds[3]))
      {
      intersectionFoundLineTip1 = this->GetIntersectionWithSliceViewBoundaries(intersectionLineTip1_2D, sliceIntersectionPoint_XY_2D,
                                                                                  sliceViewBounds, intersectionLineTip1_2D);
      }
    if ((intersectionLineTip2[0] < sliceViewBounds[0]) || // If line tip 2 is outside the FOV
      (intersectionLineTip2[0] > sliceViewBounds[1]) ||
      (intersectionLineTip2[1] < sliceViewBounds[2]) ||
      (intersectionLineTip2[1] > sliceViewBounds[3]))
      {
      intersectionFoundLineTip2 = this->GetIntersectionWithSliceViewBoundaries(intersectionLineTip2_2D, sliceIntersectionPoint_XY_2D,
                                                                                  sliceViewBounds, intersectionLineTip2_2D);
      }
    }

  // Get inner intersection line tips according to the selected visualization mode
  double intersectionInnerLineTip1[3] = { 0.0, 0.0, 0.0 };
  double intersectionInnerLineTip2[3] = { 0.0, 0.0, 0.0 };
  double sliceViewWidth = sliceViewBounds[1] - sliceViewBounds[0];
  if (VISUALIZATION_MODE == ShowIntersection)
    {
    intersectionInnerLineTip1[0] = sliceIntersectionPoint_XY[0];
    intersectionInnerLineTip1[1] = sliceIntersectionPoint_XY[1];
    intersectionInnerLineTip2[0] = sliceIntersectionPoint_XY[0];
    intersectionInnerLineTip2[1] = sliceIntersectionPoint_XY[1];
    }
  else if (VISUALIZATION_MODE == HideIntersection)
    {
    double intersectionPointToOuterLineTip1[3] = { 0.0, 0.0, 0.0 };
    double intersectionPointToOuterLineTip2[3] = { 0.0, 0.0, 0.0 };
    intersectionPointToOuterLineTip1[0] = intersectionLineTip1[0] - sliceIntersectionPoint_XY_3D[0];
    intersectionPointToOuterLineTip1[1] = intersectionLineTip1[1] - sliceIntersectionPoint_XY_3D[1];
    intersectionPointToOuterLineTip1[2] = intersectionLineTip1[2] - sliceIntersectionPoint_XY_3D[2];
    intersectionPointToOuterLineTip2[0] = intersectionLineTip2[0] - sliceIntersectionPoint_XY_3D[0];
    intersectionPointToOuterLineTip2[1] = intersectionLineTip2[1] - sliceIntersectionPoint_XY_3D[1];
    intersectionPointToOuterLineTip2[2] = intersectionLineTip2[2] - sliceIntersectionPoint_XY_3D[2];
    vtkMath::Normalize(intersectionPointToOuterLineTip1);
    vtkMath::Normalize(intersectionPointToOuterLineTip2);
    double gapSize = sliceViewWidth * HIDE_INTERSECTION_GAP_SIZE; // gap size computed according to slice view size
    intersectionInnerLineTip1[0] = sliceIntersectionPoint_XY_3D[0] + intersectionPointToOuterLineTip1[0] * gapSize;
    intersectionInnerLineTip1[1] = sliceIntersectionPoint_XY_3D[1] + intersectionPointToOuterLineTip1[1] * gapSize;
    intersectionInnerLineTip2[0] = sliceIntersectionPoint_XY_3D[0] + intersectionPointToOuterLineTip2[0] * gapSize;
    intersectionInnerLineTip2[1] = sliceIntersectionPoint_XY_3D[1] + intersectionPointToOuterLineTip2[1] * gapSize;
    }
  else
    {
    vtkWarningMacro("vtkMRMLSliceIntersectionInteractionRepresentation::UpdateSliceIntersectionDisplay failed: unknown visualization mode.");
    return;
    }

  // Define intersection lines
  pipeline->IntersectionLine1->SetPoint1(intersectionLineTip1);
  pipeline->IntersectionLine1->SetPoint2(intersectionInnerLineTip1);
  pipeline->IntersectionLine2->SetPoint1(intersectionLineTip2);
  pipeline->IntersectionLine2->SetPoint2(intersectionInnerLineTip2);

  // Set translation handle position
  vtkNew<vtkPoints> translationHandlePoints;
  double translationHandlePosition[3] = { sliceIntersectionPoint_XY[0], sliceIntersectionPoint_XY[1], 0.0 };
  pipeline->TranslationOuterHandle->SetCenter(translationHandlePosition[0], translationHandlePosition[1], translationHandlePosition[2]);
  pipeline->TranslationInnerHandle->SetCenter(translationHandlePosition[0], translationHandlePosition[1], translationHandlePosition[2]);
  translationHandlePoints->InsertNextPoint(sliceIntersectionPoint_XY_3D);
  pipeline->TranslationHandlePoints->SetPoints(translationHandlePoints);

  // Set position of rotation handles
  double rotationHandle1Position2D[2] = { intersectionLineTip1_2D[0], intersectionLineTip1_2D[1] };
  double rotationHandle2Position2D[2] = { intersectionLineTip2_2D[0], intersectionLineTip2_2D[1] };
  double rotationHandle1Orientation2D[2] = { intersectionInnerLineTip1[1] - intersectionLineTip1[1],
                                             intersectionLineTip1[0] - intersectionInnerLineTip1[0] }; // perpendicular to intersection line segment
  double rotationHandle2Orientation2D[2] = { intersectionInnerLineTip2[1] - intersectionLineTip2[1],
                                             intersectionLineTip2[0] - intersectionInnerLineTip2[0] }; // perpendicular to intersection line segment
  vtkNew<vtkMatrix4x4> rotationHandle1ToWorldMatrix, rotationHandle2ToWorldMatrix; // Compute transformation matrix (rotation + translation)
  this->ComputeHandleToWorldTransformMatrix(rotationHandle1Position2D, rotationHandle1Orientation2D, rotationHandle1ToWorldMatrix);
  this->ComputeHandleToWorldTransformMatrix(rotationHandle2Position2D, rotationHandle2Orientation2D, rotationHandle2ToWorldMatrix);
  pipeline->RotationHandle1ToWorldTransform->Identity();
  pipeline->RotationHandle1ToWorldTransform->SetMatrix(rotationHandle1ToWorldMatrix); // Update handles to world transform
  pipeline->RotationHandle2ToWorldTransform->Identity();
  pipeline->RotationHandle2ToWorldTransform->SetMatrix(rotationHandle2ToWorldMatrix); // Update handles to world transform

  // Update rotation handle points
  vtkNew<vtkPoints> rotationHandlePoints;
  double rotationHandle1Point[3] = { 0.0, 0.0, 0.0 };
  double rotationHandle1Point_h[4] = { 0.0, 0.0, 0.0, 1.0 }; //homogeneous coordinates
  for (int i = 0; i < pipeline->RotationHandle1Points->GetNumberOfPoints(); i++)
    { // Handle 1
    pipeline->RotationHandle1Points->GetPoint(i, rotationHandle1Point);
    rotationHandle1Point_h[0] = rotationHandle1Point[0];
    rotationHandle1Point_h[1] = rotationHandle1Point[1];
    rotationHandle1Point_h[2] = rotationHandle1Point[2];
    rotationHandle1ToWorldMatrix->MultiplyPoint(rotationHandle1Point_h, rotationHandle1Point_h);
    rotationHandle1Point[0] = rotationHandle1Point_h[0];
    rotationHandle1Point[1] = rotationHandle1Point_h[1];
    rotationHandle1Point[2] = rotationHandle1Point_h[2];
    rotationHandlePoints->InsertNextPoint(rotationHandle1Point);
    }
  double rotationHandle2Point[3] = { 0.0, 0.0, 0.0 };
  double rotationHandle2Point_h[4] = { 0.0, 0.0, 0.0, 1.0 }; //homogeneous coordinates
  for (int j = 0; j < pipeline->RotationHandle2Points->GetNumberOfPoints(); j++)
    { // Handle 2
    pipeline->RotationHandle2Points->GetPoint(j, rotationHandle2Point);
    rotationHandle2Point_h[0] = rotationHandle2Point[0];
    rotationHandle2Point_h[1] = rotationHandle2Point[1];
    rotationHandle2Point_h[2] = rotationHandle2Point[2];
    rotationHandle2ToWorldMatrix->MultiplyPoint(rotationHandle2Point_h, rotationHandle2Point_h);
    rotationHandle2Point[0] = rotationHandle2Point_h[0];
    rotationHandle2Point[1] = rotationHandle2Point_h[1];
    rotationHandle2Point[2] = rotationHandle2Point_h[2];
    rotationHandlePoints->InsertNextPoint(rotationHandle2Point);
    }
  pipeline->RotationHandlePoints->SetPoints(rotationHandlePoints);

  // Set position of slice offset handles
  double sliceOffsetHandle1Position2D[2] = { (rotationHandle1Position2D[0] + sliceIntersectionPoint_XY[0]) / 2 ,
                                             (rotationHandle1Position2D[1] + sliceIntersectionPoint_XY[1]) / 2 };
  double sliceOffsetHandle2Position2D[2] = { (rotationHandle2Position2D[0] + sliceIntersectionPoint_XY[0]) / 2 ,
                                             (rotationHandle2Position2D[1] + sliceIntersectionPoint_XY[1]) / 2 };
  double sliceOffsetHandleOrientation2D[2] = { intersectionLineTip2[1] - intersectionLineTip1[1],
                                               intersectionLineTip1[0] - intersectionLineTip2[0] }; // perpendicular to intersection line
  vtkNew<vtkMatrix4x4> sliceOffsetHandle1ToWorldMatrix, sliceOffsetHandle2ToWorldMatrix; // Compute transformation matrix (rotation + translation)
  this->ComputeHandleToWorldTransformMatrix(sliceOffsetHandle1Position2D, sliceOffsetHandleOrientation2D, sliceOffsetHandle1ToWorldMatrix);
  this->ComputeHandleToWorldTransformMatrix(sliceOffsetHandle2Position2D, sliceOffsetHandleOrientation2D, sliceOffsetHandle2ToWorldMatrix);
  pipeline->SliceOffsetHandle1ToWorldTransform->Identity();
  pipeline->SliceOffsetHandle1ToWorldTransform->SetMatrix(sliceOffsetHandle1ToWorldMatrix); // Update handles to world transform
  pipeline->SliceOffsetHandle2ToWorldTransform->Identity();
  pipeline->SliceOffsetHandle2ToWorldTransform->SetMatrix(sliceOffsetHandle2ToWorldMatrix); // Update handles to world transform

  // Update slice offset handle points
  vtkNew<vtkPoints> sliceOffsetHandlePoints;
  double sliceOffsetHandlePoint[3] = { 0.0, 0.0, 0.0 };
  double sliceOffsetHandlePoint_h[4] = { 0.0, 0.0, 0.0, 1.0 }; //homogeneous coordinates
  for (int i = 0; i < pipeline->SliceOffsetHandle1Points->GetNumberOfPoints(); i++)
    { // Handle 1
    pipeline->SliceOffsetHandle1Points->GetPoint(i, sliceOffsetHandlePoint);
    sliceOffsetHandlePoint_h[0] = sliceOffsetHandlePoint[0];
    sliceOffsetHandlePoint_h[1] = sliceOffsetHandlePoint[1];
    sliceOffsetHandlePoint_h[2] = sliceOffsetHandlePoint[2];
    sliceOffsetHandle1ToWorldMatrix->MultiplyPoint(sliceOffsetHandlePoint_h, sliceOffsetHandlePoint_h);
    sliceOffsetHandlePoint[0] = sliceOffsetHandlePoint_h[0];
    sliceOffsetHandlePoint[1] = sliceOffsetHandlePoint_h[1];
    sliceOffsetHandlePoint[2] = sliceOffsetHandlePoint_h[2];
    sliceOffsetHandlePoints->InsertNextPoint(sliceOffsetHandlePoint);
    }
  for (int j = 0; j < pipeline->SliceOffsetHandle2Points->GetNumberOfPoints(); j++)
    { // Handle 2
    pipeline->SliceOffsetHandle2Points->GetPoint(j, sliceOffsetHandlePoint);
    sliceOffsetHandlePoint_h[0] = sliceOffsetHandlePoint[0];
    sliceOffsetHandlePoint_h[1] = sliceOffsetHandlePoint[1];
    sliceOffsetHandlePoint_h[2] = sliceOffsetHandlePoint[2];
    sliceOffsetHandle2ToWorldMatrix->MultiplyPoint(sliceOffsetHandlePoint_h, sliceOffsetHandlePoint_h);
    sliceOffsetHandlePoint[0] = sliceOffsetHandlePoint_h[0];
    sliceOffsetHandlePoint[1] = sliceOffsetHandlePoint_h[1];
    sliceOffsetHandlePoint[2] = sliceOffsetHandlePoint_h[2];
    sliceOffsetHandlePoints->InsertNextPoint(sliceOffsetHandlePoint);
    }
  pipeline->SliceOffsetHandlePoints->SetPoints(sliceOffsetHandlePoints);

  // Store rotation and translation handle positions
  vtkNew<vtkPoints> handlePoints;
  handlePoints->InsertNextPoint(translationHandlePosition);
  handlePoints->InsertNextPoint(rotationHandle1Position2D);
  handlePoints->InsertNextPoint(rotationHandle2Position2D);
  handlePoints->InsertNextPoint(sliceOffsetHandle1Position2D);
  handlePoints->InsertNextPoint(sliceOffsetHandle2Position2D);

  // Define points along intersection line for interaction
  vtkPoints* line1PointsDefault;
  vtkPoints* line2PointsDefault;
  line1PointsDefault = pipeline->IntersectionLine1->GetOutput()->GetPoints();
  line2PointsDefault = pipeline->IntersectionLine2->GetOutput()->GetPoints();

  // Filter out those line points near interaction handles
  // If this is not done, when clicking over the handle, interaction with line (right below) could occur instead.
  vtkIdType numLinePoints;
  vtkIdType numHandlePoints;
  double linePoint[3] = { 0.0, 0.0, 0.0 };
  double handlePoint[3] = { 0.0, 0.0, 0.0 };
  bool closeToHandle;
  double linePointToHandlePoint[3] = { 0.0, 0.0, 0.0 };
  double distanceLinePointToHandlePoint;
  // Intersection line 1
  vtkNew<vtkPoints> line1PointsFiltered;
  numLinePoints = line1PointsDefault->GetNumberOfPoints();
  numHandlePoints = handlePoints->GetNumberOfPoints();
  for (int i = 0; i < numLinePoints; i++)
    {
    line1PointsDefault->GetPoint(i, linePoint);
    closeToHandle = false;
    for (int j = 0; j < numHandlePoints; j++)
      {
      handlePoints->GetPoint(j, handlePoint);
      linePointToHandlePoint[0] = handlePoint[0] - linePoint[0];
      linePointToHandlePoint[1] = handlePoint[1] - linePoint[1];
      linePointToHandlePoint[2] = handlePoint[2] - linePoint[2];
      distanceLinePointToHandlePoint = vtkMath::Norm(linePointToHandlePoint);
      if (distanceLinePointToHandlePoint < LINE_POINTS_FILTERING_THRESHOLD)
        {
        closeToHandle = true;
        break;
        }
      }
    if (!closeToHandle)
      {
      line1PointsFiltered->InsertNextPoint(linePoint);
      }
    }
  pipeline->IntersectionLine1Points->SetPoints(line1PointsFiltered);

  // Intersection line 2
  vtkNew<vtkPoints> line2PointsFiltered;
  numLinePoints = line2PointsDefault->GetNumberOfPoints();
  numHandlePoints = handlePoints->GetNumberOfPoints();
  for (int i = 0; i < numLinePoints; i++)
    {
    line2PointsDefault->GetPoint(i, linePoint);
    closeToHandle = false;
    for (int j = 0; j < numHandlePoints; j++)
      {
      handlePoints->GetPoint(j, handlePoint);
      linePointToHandlePoint[0] = handlePoint[0] - linePoint[0];
      linePointToHandlePoint[1] = handlePoint[1] - linePoint[1];
      linePointToHandlePoint[2] = handlePoint[2] - linePoint[2];
      distanceLinePointToHandlePoint = vtkMath::Norm(linePointToHandlePoint);
      if (distanceLinePointToHandlePoint < LINE_POINTS_FILTERING_THRESHOLD)
        {
        closeToHandle = true;
        break;
        }
      }
    if (!closeToHandle)
      {
      line2PointsFiltered->InsertNextPoint(linePoint);
      }
    }
  pipeline->IntersectionLine2Points->SetPoints(line2PointsFiltered);

  // Visibility
  pipeline->SetVisibility(true);
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::GetSliceViewBoundariesXY(vtkMRMLSliceNode* sliceNode, double* sliceViewBounds)
{
  // Get FOV of current slice node in mm
  char* sliceNodeName = sliceNode->GetName();
  double sliceFOVMm[3] = { 0.0,0.0,0.0 };
  sliceNode->GetFieldOfView(sliceFOVMm);

  // Get XYToRAS and RASToXY transform matrices
  vtkMatrix4x4* currentXYToRAS = sliceNode->GetXYToRAS();
  vtkNew<vtkMatrix4x4> currentRASToXY;
  vtkMatrix4x4::Invert(currentXYToRAS, currentRASToXY);

  // Get slice view axes in RAS
  double sliceOrigin[4] = { 0.0,0.0,0.0,1.0 };
  double slicePointAxisX[4] = { 100.0,0.0,0.0,1.0 };
  double slicePointAxisY[4] = { 0.0,100.0,0.0,1.0 };
  currentXYToRAS->MultiplyPoint(sliceOrigin, sliceOrigin);
  currentXYToRAS->MultiplyPoint(slicePointAxisX, slicePointAxisX);
  currentXYToRAS->MultiplyPoint(slicePointAxisY, slicePointAxisY);
  double sliceAxisX[3] = { slicePointAxisX[0] - sliceOrigin[0],
                           slicePointAxisX[1] - sliceOrigin[1],
                           slicePointAxisX[2] - sliceOrigin[2] };
  double sliceAxisY[3] = { slicePointAxisY[0] - sliceOrigin[0],
                           slicePointAxisY[1] - sliceOrigin[1],
                           slicePointAxisY[2] - sliceOrigin[2] };
  vtkMath::Normalize(sliceAxisX);
  vtkMath::Normalize(sliceAxisY);

  // Calculate corners of FOV in RAS coordinate system
  double bottomLeftCornerRAS[4] = { 0.0,0.0,0.0,1.0 };
  double topLeftCornerRAS[4] = { 0.0,0.0,0.0,1.0 };
  double bottomRightCornerRAS[4] = { 0.0,0.0,0.0,1.0 };
  double topRightCornerRAS[4] = { 0.0,0.0,0.0,1.0 };

  // Get slice view corners RAS
  bottomLeftCornerRAS[0] = sliceOrigin[0];
  bottomLeftCornerRAS[1] = sliceOrigin[1];
  bottomLeftCornerRAS[2] = sliceOrigin[2];
  topLeftCornerRAS[0] = sliceOrigin[0] + sliceAxisY[0] * sliceFOVMm[1];
  topLeftCornerRAS[1] = sliceOrigin[1] + sliceAxisY[1] * sliceFOVMm[1];
  topLeftCornerRAS[2] = sliceOrigin[2] + sliceAxisY[2] * sliceFOVMm[1];
  bottomRightCornerRAS[0] = sliceOrigin[0] + sliceAxisX[0] * sliceFOVMm[0];
  bottomRightCornerRAS[1] = sliceOrigin[1] + sliceAxisX[1] * sliceFOVMm[0];
  bottomRightCornerRAS[2] = sliceOrigin[2] + sliceAxisX[2] * sliceFOVMm[0];
  topRightCornerRAS[0] = sliceOrigin[0] + sliceAxisY[0] * sliceFOVMm[1] + sliceAxisX[0] * sliceFOVMm[0];
  topRightCornerRAS[1] = sliceOrigin[1] + sliceAxisY[1] * sliceFOVMm[1] + sliceAxisX[1] * sliceFOVMm[0];
  topRightCornerRAS[2] = sliceOrigin[2] + sliceAxisY[2] * sliceFOVMm[1] + sliceAxisX[2] * sliceFOVMm[0];

  // Calculate corners of FOV in XY coordinate system
  double bottomLeftCornerXY[4] = { 0.0,0.0,0.0,1.0 };
  double topLeftCornerXY[4] = { 0.0,0.0,0.0,1.0 };
  double bottomRightCornerXY[4] = { 0.0,0.0,0.0,1.0 };
  double topRightCornerXY[4] = { 0.0,0.0,0.0,1.0 };
  currentRASToXY->MultiplyPoint(bottomLeftCornerRAS, bottomLeftCornerXY);
  currentRASToXY->MultiplyPoint(topLeftCornerRAS, topLeftCornerXY);
  currentRASToXY->MultiplyPoint(bottomRightCornerRAS, bottomRightCornerXY);
  currentRASToXY->MultiplyPoint(topRightCornerRAS, topRightCornerXY);

  // Get slice view range XY
  sliceViewBounds[0] = bottomLeftCornerXY[0]; // Min value of X
  sliceViewBounds[1] = topRightCornerXY[0]; // Max value of X
  sliceViewBounds[2] = bottomLeftCornerXY[1]; //  Min value of Y
  sliceViewBounds[3] = topRightCornerXY[1]; //  Max value of Y
}

//----------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionRepresentation::GetIntersectionWithSliceViewBoundaries(double* pointA, double* pointB,
      double* sliceViewBounds, double* intersectionPoint)
{
    // Get line equation -> y = slope * x + intercept
    double xA = pointA[0];
    double yA = pointA[1];
    double xB = pointB[0];
    double yB = pointB[1];
    double dx, dy, slope, intercept;
    dx = xB - xA;
    dy = yB - yA;
    slope = dy / dx;
    intercept = yA - slope * xA;

    // Get line bounding box
    double lineBounds[4] = { std::min(xA, xB), std::max(xA, xB), std::min(yA, yB), std::max(yA, yB) };

    // Slice view bounds
    double xMin = sliceViewBounds[0];
    double xMax = sliceViewBounds[1];
    double yMin = sliceViewBounds[2];
    double yMax = sliceViewBounds[3];

    // Get intersection point using line equation
    double x0, y0;
    if ((sliceViewBounds[0] > lineBounds[0]) && (sliceViewBounds[0] < lineBounds[1]))
      {
      y0 = slope * sliceViewBounds[0] + intercept;
      if ((y0 > sliceViewBounds[2]) && (y0 < sliceViewBounds[3]))
        {
        intersectionPoint[0] = sliceViewBounds[0];
        intersectionPoint[1] = y0;
        return true;
        }
      }
    if ((sliceViewBounds[1] > lineBounds[0]) && (sliceViewBounds[1] < lineBounds[1]))
      {
      y0 = slope * sliceViewBounds[1] + intercept;
      if ((y0 > sliceViewBounds[2]) && (y0 < sliceViewBounds[3]))
        {
        intersectionPoint[0] = sliceViewBounds[1];
        intersectionPoint[1] = y0;
        return true;
        }
      }
    if ((sliceViewBounds[2] > lineBounds[2]) && (sliceViewBounds[2] < lineBounds[3]))
      {
      if (std::isfinite(slope)) // check if slope is finite
        {
        x0 = (sliceViewBounds[2] - intercept)/slope;
        if ((x0 > sliceViewBounds[0]) && (x0 < sliceViewBounds[1]))
          {
          intersectionPoint[0] = x0;
          intersectionPoint[1] = sliceViewBounds[2];
          return true;
          }
        }
      else // infinite slope = vertical line
        {
          intersectionPoint[0] = lineBounds[0]; // or lineBounds[1] (if the line is vertical, then both points A and B have the same value of X)
          intersectionPoint[1] = sliceViewBounds[2];
          return true;
        }
      }
    if ((sliceViewBounds[3] > lineBounds[2]) && (sliceViewBounds[3] < lineBounds[3]))
      {
      if (std::isfinite(slope)) // check if slope is finite
        {
        x0 = (sliceViewBounds[3] - intercept)/slope;
        if ((x0 > sliceViewBounds[0]) && (x0 < sliceViewBounds[1]))
          {
          intersectionPoint[0] = x0;
          intersectionPoint[1] = sliceViewBounds[3];
          return true;
          }
        }
      else // infinite slope = vertical line
        {
          intersectionPoint[0] = lineBounds[0]; // or lineBounds[1] (if the line is vertical, then both points A and B have the same value of X)
          intersectionPoint[1] = sliceViewBounds[3];
          return true;
        }
      }
    return false;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetSliceNode(vtkMRMLSliceNode * sliceNode)
{
  if (sliceNode == this->Internal->SliceNode)
    {
    // no change
    return;
    }
  if (this->Internal->SliceNode)
    {
    this->Internal->SliceNode->RemoveObserver(this->Internal->SliceNodeModifiedCommand);
    }
  if (sliceNode)
    {
    sliceNode->AddObserver(vtkCommand::ModifiedEvent, this->Internal->SliceNodeModifiedCommand.GetPointer());
    }
  this->Internal->SliceNode = sliceNode;
  this->UpdateIntersectingSliceNodes();
}

//----------------------------------------------------------------------
vtkMRMLSliceNode* vtkMRMLSliceIntersectionInteractionRepresentation::GetSliceNode()
{
  return this->Internal->SliceNode;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::AddIntersectingSliceLogic(vtkMRMLSliceLogic * sliceLogic)
{
  if (!sliceLogic)
    {
    return;
    }
  if (sliceLogic->GetSliceNode() == this->Internal->SliceNode)
    {
    // it is the slice itself, not an intersecting slice
    return;
    }
  if (this->GetDisplayPipelineFromSliceLogic(sliceLogic))
    {
    // slice node already added
    return;
    }

  SliceIntersectionInteractionDisplayPipeline* pipeline = new SliceIntersectionInteractionDisplayPipeline;
  pipeline->SetAndObserveSliceLogic(sliceLogic, this->Internal->SliceNodeModifiedCommand);
  pipeline->AddActors(this->Renderer);
  this->Internal->SliceIntersectionInteractionDisplayPipelines.push_back(pipeline);
  this->UpdateSliceIntersectionDisplay(pipeline);
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::RemoveIntersectingSliceNode(vtkMRMLSliceNode * sliceNode)
{
  if (!sliceNode)
    {
    return;
    }
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    if (!(*sliceIntersectionIt)->SliceLogic)
      {
      continue;
      }
    if (sliceNode == (*sliceIntersectionIt)->SliceLogic->GetSliceNode())
      {
      // found it
      (*sliceIntersectionIt)->RemoveActors(this->Renderer);
      delete (*sliceIntersectionIt);
      this->Internal->SliceIntersectionInteractionDisplayPipelines.erase(sliceIntersectionIt);
      break;
      }
    }
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::UpdateIntersectingSliceNodes()
{
  this->RemoveAllIntersectingSliceNodes();
  if (!this->GetSliceNode() || !this->MRMLApplicationLogic)
    {
    return;
    }
  vtkCollection* sliceLogics = this->MRMLApplicationLogic->GetSliceLogics();
  if (!sliceLogics)
    {
    return;
    }
  vtkMRMLSliceLogic* sliceLogic;
  vtkCollectionSimpleIterator it;
  for (sliceLogics->InitTraversal(it);
    (sliceLogic = vtkMRMLSliceLogic::SafeDownCast(sliceLogics->GetNextItemAsObject(it)));)
    {
    this->AddIntersectingSliceLogic(sliceLogic);
    }
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::RemoveAllIntersectingSliceNodes()
{
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    (*sliceIntersectionIt)->RemoveActors(this->Renderer);
    delete (*sliceIntersectionIt);
    }
  this->Internal->SliceIntersectionInteractionDisplayPipelines.clear();
}

//----------------------------------------------------------------------
double* vtkMRMLSliceIntersectionInteractionRepresentation::GetSliceIntersectionPoint()
{
  // Slice node where interaction takes place
  vtkMRMLSliceNode* intersectingSliceNode = this->Internal->SliceNode;

  // Get slice nodes from scene
  vtkMRMLScene* scene = intersectingSliceNode->GetScene();
  if (!scene)
    {
    return this->SliceIntersectionPoint;
    }
  std::vector<vtkMRMLNode*> sliceNodes;
  int nnodes = scene ? scene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes) : 0;

  // Red slice
  vtkMRMLSliceNode* redSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[0]);
  vtkMatrix4x4* redSliceToRASTransformMatrix = redSliceNode->GetSliceToRAS();
  double redSlicePlaneOrigin[3] = { redSliceToRASTransformMatrix->GetElement(0, 3), redSliceToRASTransformMatrix->GetElement(1,3),
                                    redSliceToRASTransformMatrix->GetElement(2,3) };
  double redSlicePlaneNormal[3] = { redSliceToRASTransformMatrix->GetElement(0, 2), redSliceToRASTransformMatrix->GetElement(1,2),
                                    redSliceToRASTransformMatrix->GetElement(2,2) };

  // Green slice
  vtkMRMLSliceNode* greenSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[1]);
  vtkMatrix4x4* greenSliceToRASTransformMatrix = greenSliceNode->GetSliceToRAS();
  double greenSlicePlaneOrigin[3] = { greenSliceToRASTransformMatrix->GetElement(0, 3), greenSliceToRASTransformMatrix->GetElement(1,3),
                                      greenSliceToRASTransformMatrix->GetElement(2,3) };
  double greenSlicePlaneNormal[3] = { greenSliceToRASTransformMatrix->GetElement(0, 2), greenSliceToRASTransformMatrix->GetElement(1,2),
                                      greenSliceToRASTransformMatrix->GetElement(2,2) };

  // Yellow slice
  vtkMRMLSliceNode* yellowSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[2]);
  vtkMatrix4x4* yellowSliceToRASTransformMatrix = yellowSliceNode->GetSliceToRAS();
  double yellowSlicePlaneOrigin[3] = { yellowSliceToRASTransformMatrix->GetElement(0, 3), yellowSliceToRASTransformMatrix->GetElement(1,3),
                                       yellowSliceToRASTransformMatrix->GetElement(2,3) };
  double yellowSlicePlaneNormal[3] = { yellowSliceToRASTransformMatrix->GetElement(0, 2), yellowSliceToRASTransformMatrix->GetElement(1,2),
                                       yellowSliceToRASTransformMatrix->GetElement(2,2) };

  // Build matrix with normals (n1,n2,n3) and compute determinant det(n1,n2,n3)
  vtkNew<vtkMatrix3x3> slicePlaneNormalsMatrix;
  slicePlaneNormalsMatrix->SetElement(0, 0, redSlicePlaneNormal[0]);
  slicePlaneNormalsMatrix->SetElement(0, 1, redSlicePlaneNormal[1]);
  slicePlaneNormalsMatrix->SetElement(0, 2, redSlicePlaneNormal[2]);
  slicePlaneNormalsMatrix->SetElement(1, 0, greenSlicePlaneNormal[0]);
  slicePlaneNormalsMatrix->SetElement(1, 1, greenSlicePlaneNormal[1]);
  slicePlaneNormalsMatrix->SetElement(1, 2, greenSlicePlaneNormal[2]);
  slicePlaneNormalsMatrix->SetElement(2, 0, yellowSlicePlaneNormal[0]);
  slicePlaneNormalsMatrix->SetElement(2, 1, yellowSlicePlaneNormal[1]);
  slicePlaneNormalsMatrix->SetElement(2, 2, yellowSlicePlaneNormal[2]);
  double determinant = slicePlaneNormalsMatrix->Determinant();

  // 3-plane intersection point: P = (( point_on1 � n1 )( n2 � n3 ) + ( point_on2 � n2 )( n3 � n1 ) + ( point_on3 � n3 )( n1 � n2 )) / det(n1,n2,n3)
  double d1 = vtkMath::Dot(redSlicePlaneOrigin, redSlicePlaneNormal);
  double cross1[3];
  vtkMath::Cross(greenSlicePlaneNormal, yellowSlicePlaneNormal, cross1);
  vtkMath::MultiplyScalar(cross1, d1);
  double d2 = vtkMath::Dot(greenSlicePlaneOrigin, greenSlicePlaneNormal);
  double cross2[3];
  vtkMath::Cross(yellowSlicePlaneNormal, redSlicePlaneNormal, cross2);
  vtkMath::MultiplyScalar(cross2, d2);
  double d3 = vtkMath::Dot(yellowSlicePlaneOrigin, yellowSlicePlaneNormal);
  double cross3[3];
  vtkMath::Cross(redSlicePlaneNormal, greenSlicePlaneNormal, cross3);
  vtkMath::MultiplyScalar(cross3, d3);
  double crossSum12[3];
  vtkMath::Add(cross1, cross2, crossSum12);
  double sliceIntersectionPoint[3];
  vtkMath::Add(crossSum12, cross3, sliceIntersectionPoint);
  vtkMath::MultiplyScalar(sliceIntersectionPoint, 1 / determinant);

  this->SliceIntersectionPoint[0] = sliceIntersectionPoint[0];
  this->SliceIntersectionPoint[1] = sliceIntersectionPoint[1];
  this->SliceIntersectionPoint[2] = sliceIntersectionPoint[2];

  return this->SliceIntersectionPoint;
}

//---------------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::IntersectWithFinitePlane(double n[3], double o[3],
  double pOrigin[3], double px[3], double py[3],
  double x0[3], double x1[3])
{
  // Since we are dealing with convex shapes, if there is an intersection a
  // single line is produced as output. So all this is necessary is to
  // intersect the four bounding lines of the finite line and find the two
  // intersection points.
  int numInts = 0;
  double t, * x = x0;
  double xr0[3], xr1[3];

  // First line
  xr0[0] = pOrigin[0];
  xr0[1] = pOrigin[1];
  xr0[2] = pOrigin[2];
  xr1[0] = px[0];
  xr1[1] = px[1];
  xr1[2] = px[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
    {
    numInts++;
    x = x1;
    }

  // Second line
  xr1[0] = py[0];
  xr1[1] = py[1];
  xr1[2] = py[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
    {
    numInts++;
    x = x1;
    }
  if (numInts == 2)
    {
    return 1;
    }

  // Third line
  xr0[0] = -pOrigin[0] + px[0] + py[0];
  xr0[1] = -pOrigin[1] + px[1] + py[1];
  xr0[2] = -pOrigin[2] + px[2] + py[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
    {
    numInts++;
    x = x1;
    }
  if (numInts == 2)
    {
    return 1;
    }

  // Fourth and last line
  xr1[0] = px[0];
  xr1[1] = px[1];
  xr1[2] = px[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
    {
    numInts++;
    }
  if (numInts == 2)
    {
    return 1;
    }

  // No intersection has occurred, or a single degenerate point
  return 0;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::TransformIntersectingSlices(vtkMatrix4x4 * rotatedSliceToSliceTransformMatrix)
{
  std::deque<int> wasModified;
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    if (!(*sliceIntersectionIt)
      || !(*sliceIntersectionIt)->GetVisibility())
      {
      continue;
      }
    vtkMRMLSliceNode* sliceNode = (*sliceIntersectionIt)->SliceLogic->GetSliceNode();
    wasModified.push_back(sliceNode->StartModify());

    vtkNew<vtkMatrix4x4> rotatedSliceToRAS;
    vtkMatrix4x4::Multiply4x4(rotatedSliceToSliceTransformMatrix, sliceNode->GetSliceToRAS(),
      rotatedSliceToRAS);

    sliceNode->GetSliceToRAS()->DeepCopy(rotatedSliceToRAS);
    }

  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    if (!(*sliceIntersectionIt)
      || !(*sliceIntersectionIt)->GetVisibility())
      {
      continue;
      }
    vtkMRMLSliceNode* sliceNode = (*sliceIntersectionIt)->SliceLogic->GetSliceNode();
    sliceNode->UpdateMatrices();
    sliceNode->EndModify(wasModified.front());
    wasModified.pop_front();
    }
}

//----------------------------------------------------------------------
double vtkMRMLSliceIntersectionInteractionRepresentation::GetMaximumHandlePickingDistance2()
{
  double maximumHandlePickingDistance = this->InteractionSize / 2.0 + this->PickingTolerance * this->ScreenScaleFactor;
  return maximumHandlePickingDistance * maximumHandlePickingDistance;
}

//-----------------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::CanInteract(vtkMRMLInteractionEventData* interactionEventData,
  int& foundComponentType, int& foundComponentIndex, int& intersectingSliceNodeIndex, double& closestDistance2, double& handleOpacity)
{
  foundComponentType = InteractionNone;
  vtkMRMLAbstractViewNode* viewNode = this->GetViewNode();
  /*
  if (!viewNode || !this->GetVisibility() || !interactionEventData)
    {
    return;
    }
  */
  closestDistance2 = VTK_DOUBLE_MAX; // in display coordinate system
  foundComponentIndex = -1;
  intersectingSliceNodeIndex = -1;
  handleOpacity = 0.0;

  double maxPickingDistanceFromControlPoint2 = this->GetMaximumHandlePickingDistance2();
  double extendedPickingDistanceFromControlPoint2 = maxPickingDistanceFromControlPoint2 + OPACITY_RANGE;
  double displayPosition3[3] = { 0.0, 0.0, 0.0 };
  // Display position is valid in case of desktop interactions. Otherwise it is a 3D only context such as
  // virtual reality, and then we expect a valid world position in the absence of display position.
  if (interactionEventData->IsDisplayPositionValid())
    {
    const int* displayPosition = interactionEventData->GetDisplayPosition();
    displayPosition3[0] = static_cast<double>(displayPosition[0]);
    displayPosition3[1] = static_cast<double>(displayPosition[1]);
    }
  else if (!interactionEventData->IsWorldPositionValid())
    {
    return;
    }

  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    if (!(*sliceIntersectionIt)
      || !(*sliceIntersectionIt)->GetVisibility())
      {
      continue;
      }
    vtkMRMLSliceNode* sliceNode = this->GetSliceNode();
    if (sliceNode)
      {
      double handleDisplayPos[4] = { 0.0, 0.0, 0.0, 1.0 };

      vtkNew<vtkMatrix4x4> rasToxyMatrix;
      vtkMatrix4x4::Invert(sliceNode->GetXYToRAS(), rasToxyMatrix);

      bool handlePicked = false;
      double opacity = 0.0;
      HandleInfoList handleInfoList = this->GetHandleInfoList((*sliceIntersectionIt));
      for (HandleInfo handleInfo : handleInfoList)
        {
        double* handleWorldPos = handleInfo.PositionWorld;
        rasToxyMatrix->MultiplyPoint(handleWorldPos, handleDisplayPos);
        handleDisplayPos[2] = displayPosition3[2]; // Handles are always projected
        double dist2 = vtkMath::Distance2BetweenPoints(handleDisplayPos, displayPosition3);
        if (dist2 < extendedPickingDistanceFromControlPoint2)
          {
          if (dist2 < maxPickingDistanceFromControlPoint2)
            {
            handleOpacity = 1.0;
            if (dist2 < closestDistance2)
              {
              closestDistance2 = dist2;
              foundComponentType = handleInfo.ComponentType;
              foundComponentIndex = handleInfo.Index;
              intersectingSliceNodeIndex = handleInfo.IntersectingSliceNodeIndex;
              handlePicked = true;
              }
            }
          else
            {
            opacity = (dist2 - extendedPickingDistanceFromControlPoint2) / ( - OPACITY_RANGE);
            if (opacity > handleOpacity)
              {
              handleOpacity = opacity;
              }
            }
          }
        }
      }
    else
      {
      bool handlePicked = false;
      HandleInfoList handleInfoList = this->GetHandleInfoList((*sliceIntersectionIt));
      for (HandleInfo handleInfo : handleInfoList)
        {
        double* handleWorldPos = handleInfo.PositionWorld;
        double handleDisplayPos[3] = { 0 };

        if (interactionEventData->IsDisplayPositionValid())
          {
          double pixelTolerance = this->InteractionSize / 2.0 / this->GetViewScaleFactorAtPosition(handleWorldPos)
            + this->PickingTolerance * this->ScreenScaleFactor;
          this->Renderer->SetWorldPoint(handleWorldPos);
          this->Renderer->WorldToDisplay();
          this->Renderer->GetDisplayPoint(handleDisplayPos);
          handleDisplayPos[2] = 0.0;
          double dist2 = vtkMath::Distance2BetweenPoints(handleDisplayPos, displayPosition3);
          if (dist2 < pixelTolerance * pixelTolerance && dist2 < closestDistance2)
            {
            closestDistance2 = dist2;
            foundComponentType = handleInfo.ComponentType;
            foundComponentIndex = handleInfo.Index;
            intersectingSliceNodeIndex = handleInfo.IntersectingSliceNodeIndex;
            handlePicked = true;
            }
          }
        else
          {
          const double* worldPosition = interactionEventData->GetWorldPosition();
          double worldTolerance = this->InteractionSize / 2.0 +
            this->PickingTolerance / interactionEventData->GetWorldToPhysicalScale();
          double dist2 = vtkMath::Distance2BetweenPoints(handleWorldPos, worldPosition);
          if (dist2 < worldTolerance * worldTolerance && dist2 < closestDistance2)
            {
            closestDistance2 = dist2;
            foundComponentType = handleInfo.ComponentType;
            foundComponentIndex = handleInfo.Index;
            intersectingSliceNodeIndex = handleInfo.IntersectingSliceNodeIndex;
            }
          }
        }
      }
    }
  }

//----------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::HandleInfoList
vtkMRMLSliceIntersectionInteractionRepresentation::GetHandleInfoList(SliceIntersectionInteractionDisplayPipeline* pipeline)
{
  vtkMRMLSliceNode* currentSliceNode = this->GetSliceNode(); // Get slice node
  vtkMRMLSliceNode* intersectingSliceNode = pipeline->SliceLogic->GetSliceNode(); // Get intersecting slice node
  int intersectingSliceNodeIndex = this->GetSliceNodeIndex(intersectingSliceNode);
  vtkMatrix4x4* currentXYToRAS = currentSliceNode->GetXYToRAS(); // Get XY to RAS transform matrix
  HandleInfoList handleInfoList;
  for (int i = 0; i < pipeline->RotationHandlePoints->GetNumberOfPoints(); ++i)
    {
    double handlePositionLocal[3] = { 0 };
    double handlePositionLocal_h[4] = { 0.0,0.0,0.0,1.0 };
    double handlePositionWorld[3] = { 0 };
    double handlePositionWorld_h[4] = { 0.0,0.0,0.0,1.0 };
    pipeline->RotationHandlePoints->GetPoint(i, handlePositionLocal);
    handlePositionLocal_h[0] = handlePositionLocal[0];
    handlePositionLocal_h[1] = handlePositionLocal[1];
    handlePositionLocal_h[2] = handlePositionLocal[2];
    currentXYToRAS->MultiplyPoint(handlePositionLocal_h, handlePositionWorld_h);
    handlePositionWorld[0] = handlePositionWorld_h[0];
    handlePositionWorld[1] = handlePositionWorld_h[1];
    handlePositionWorld[2] = handlePositionWorld_h[2];
    HandleInfo info(i, InteractionRotationHandle, intersectingSliceNodeIndex, handlePositionWorld, handlePositionLocal);
    handleInfoList.push_back(info);
    }

  for (int i = 0; i < pipeline->TranslationHandlePoints->GetNumberOfPoints(); ++i)
    {
    double handlePositionLocal[3] = { 0 };
    double handlePositionLocal_h[4] = { 0.0,0.0,0.0,1.0 };
    double handlePositionWorld[3] = { 0 };
    double handlePositionWorld_h[4] = { 0.0,0.0,0.0,1.0 };
    pipeline->TranslationHandlePoints->GetPoint(i, handlePositionLocal);
    handlePositionLocal_h[0] = handlePositionLocal[0];
    handlePositionLocal_h[1] = handlePositionLocal[1];
    handlePositionLocal_h[2] = handlePositionLocal[2];
    currentXYToRAS->MultiplyPoint(handlePositionLocal_h, handlePositionWorld_h);
    handlePositionWorld[0] = handlePositionWorld_h[0];
    handlePositionWorld[1] = handlePositionWorld_h[1];
    handlePositionWorld[2] = handlePositionWorld_h[2];
    HandleInfo info(i, InteractionTranslationHandle, intersectingSliceNodeIndex, handlePositionWorld, handlePositionLocal);
    handleInfoList.push_back(info);
    }

  for (int i = 0; i < pipeline->SliceOffsetHandlePoints->GetNumberOfPoints(); ++i)
    {
    double handlePositionLocal[3] = { 0 };
    double handlePositionLocal_h[4] = { 0.0,0.0,0.0,1.0 };
    double handlePositionWorld[3] = { 0 };
    double handlePositionWorld_h[4] = { 0.0,0.0,0.0,1.0 };
    pipeline->SliceOffsetHandlePoints->GetPoint(i, handlePositionLocal);
    handlePositionLocal_h[0] = handlePositionLocal[0];
    handlePositionLocal_h[1] = handlePositionLocal[1];
    handlePositionLocal_h[2] = handlePositionLocal[2];
    currentXYToRAS->MultiplyPoint(handlePositionLocal_h, handlePositionWorld_h);
    handlePositionWorld[0] = handlePositionWorld_h[0];
    handlePositionWorld[1] = handlePositionWorld_h[1];
    handlePositionWorld[2] = handlePositionWorld_h[2];
    HandleInfo info(i, InteractionSliceOffsetHandle, intersectingSliceNodeIndex, handlePositionWorld, handlePositionLocal);
    handleInfoList.push_back(info);
    }

  for (int i = 0; i < pipeline->IntersectionLine1Points->GetNumberOfPoints(); ++i)
    {
    double handlePositionLocal[3] = { 0 };
    double handlePositionLocal_h[4] = { 0.0,0.0,0.0,1.0 };
    double handlePositionWorld[3] = { 0 };
    double handlePositionWorld_h[4] = { 0.0,0.0,0.0,1.0 };
    pipeline->IntersectionLine1Points->GetPoint(i, handlePositionLocal);
    handlePositionLocal_h[0] = handlePositionLocal[0];
    handlePositionLocal_h[1] = handlePositionLocal[1];
    handlePositionLocal_h[2] = handlePositionLocal[2];
    currentXYToRAS->MultiplyPoint(handlePositionLocal_h, handlePositionWorld_h);
    handlePositionWorld[0] = handlePositionWorld_h[0];
    handlePositionWorld[1] = handlePositionWorld_h[1];
    handlePositionWorld[2] = handlePositionWorld_h[2];
    HandleInfo info(i, InteractionIntersectionLine, intersectingSliceNodeIndex, handlePositionWorld, handlePositionLocal);
    handleInfoList.push_back(info);
    }

  for (int i = 0; i < pipeline->IntersectionLine2Points->GetNumberOfPoints(); ++i)
    {
    double handlePositionLocal[3] = { 0 };
    double handlePositionLocal_h[4] = { 0.0,0.0,0.0,1.0 };
    double handlePositionWorld[3] = { 0 };
    double handlePositionWorld_h[4] = { 0.0,0.0,0.0,1.0 };
    pipeline->IntersectionLine2Points->GetPoint(i, handlePositionLocal);
    handlePositionLocal_h[0] = handlePositionLocal[0];
    handlePositionLocal_h[1] = handlePositionLocal[1];
    handlePositionLocal_h[2] = handlePositionLocal[2];
    currentXYToRAS->MultiplyPoint(handlePositionLocal_h, handlePositionWorld_h);
    handlePositionWorld[0] = handlePositionWorld_h[0];
    handlePositionWorld[1] = handlePositionWorld_h[1];
    handlePositionWorld[2] = handlePositionWorld_h[2];
    HandleInfo info(i, InteractionIntersectionLine, intersectingSliceNodeIndex, handlePositionWorld, handlePositionLocal);
    handleInfoList.push_back(info);
    }

  return handleInfoList;
}

//----------------------------------------------------------------------
double vtkMRMLSliceIntersectionInteractionRepresentation::GetViewScaleFactorAtPosition(double positionWorld[3])
{
  double viewScaleFactorMmPerPixel = 1.0;
  if (!this->Renderer || !this->Renderer->GetActiveCamera())
    {
    return viewScaleFactorMmPerPixel;
    }

  vtkCamera* cam = this->Renderer->GetActiveCamera();
  if (cam->GetParallelProjection())
    {
    // Viewport: xmin, ymin, xmax, ymax; range: 0.0-1.0; origin is bottom left
    // Determine the available renderer size in pixels
    double minX = 0;
    double minY = 0;
    this->Renderer->NormalizedDisplayToDisplay(minX, minY);
    double maxX = 1;
    double maxY = 1;
    this->Renderer->NormalizedDisplayToDisplay(maxX, maxY);
    int rendererSizeInPixels[2] = { static_cast<int>(maxX - minX), static_cast<int>(maxY - minY) };
    // Parallel scale: height of the viewport in world-coordinate distances.
    // Larger numbers produce smaller images.
    viewScaleFactorMmPerPixel = (cam->GetParallelScale() * 2.0) / double(rendererSizeInPixels[1]);
    }
  else
    {
    double cameraFP[4] = { positionWorld[0], positionWorld[1], positionWorld[2], 1.0 };

    double cameraViewUp[3] = { 0 };
    cam->GetViewUp(cameraViewUp);
    vtkMath::Normalize(cameraViewUp);

    // Get distance in pixels between two points at unit distance above and below the focal point
    this->Renderer->SetWorldPoint(cameraFP[0] + cameraViewUp[0], cameraFP[1] + cameraViewUp[1], cameraFP[2] + cameraViewUp[2], cameraFP[3]);
    this->Renderer->WorldToDisplay();
    double topCenter[3] = { 0 };
    this->Renderer->GetDisplayPoint(topCenter);
    topCenter[2] = 0.0;
    this->Renderer->SetWorldPoint(cameraFP[0] - cameraViewUp[0], cameraFP[1] - cameraViewUp[1], cameraFP[2] - cameraViewUp[2], cameraFP[3]);
    this->Renderer->WorldToDisplay();
    double bottomCenter[3] = { 0 };
    this->Renderer->GetDisplayPoint(bottomCenter);
    bottomCenter[2] = 0.0;
    double distInPixels = sqrt(vtkMath::Distance2BetweenPoints(topCenter, bottomCenter));

    // if render window is not initialized yet then distInPixels == 0.0,
    // in that case just leave the default viewScaleFactorMmPerPixel
    if (distInPixels > 1e-3)
      {
      // 2.0 = 2x length of viewUp vector in mm (because viewUp is unit vector)
      viewScaleFactorMmPerPixel = 2.0 / distInPixels;
      }
    }
  return viewScaleFactorMmPerPixel;
}

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::GetSliceNodeIndex(vtkMRMLSliceNode* sliceNode)
{
  // Get red, green and yellow slice nodes
  vtkMRMLScene* scene = sliceNode->GetScene();
  std::vector<vtkMRMLNode*> sliceNodes;
  int nnodes = scene ? scene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes) : 0;
  vtkMRMLSliceNode* redSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[0]);
  vtkMRMLSliceNode* greenSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[1]);
  vtkMRMLSliceNode* yellowSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[2]);

  // Slice node index
  int sliceNodeIndex = -1;

  if (sliceNode == redSliceNode)
    {
    sliceNodeIndex = 0;
    }
  else if (sliceNode == greenSliceNode)
    {
    sliceNodeIndex = 1;
    }
  else if (sliceNode == yellowSliceNode)
    {
    sliceNodeIndex = 2;
    }

  return sliceNodeIndex;
}

//----------------------------------------------------------------------
vtkMRMLSliceNode* vtkMRMLSliceIntersectionInteractionRepresentation::GetSliceNodeFromIndex(vtkMRMLScene* scene, int sliceNodeIndex)
{
  // Get red, green and yellow slice nodes
  vtkMRMLScene* currentScene = vtkMRMLScene::SafeDownCast(scene);
  std::vector<vtkMRMLNode*> sliceNodes;
  int nnodes = currentScene ? currentScene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes) : 0;

  // Slice node
  if (sliceNodeIndex == 0)
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[0]); // red slice node
    return sliceNode;
    }
  else if (sliceNodeIndex == 1)
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[1]); // green slice node
    return sliceNode;
    }
  else if (sliceNodeIndex == 2)
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[2]); // yellow slice node
    return sliceNode;
    }
  else
    {
    vtkMRMLSliceNode* sliceNode = nullptr;
    return sliceNode;
    }
}

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::GetLineTipsFromIntersectingSliceNode(vtkMRMLSliceNode* intersectingSliceNode,
    vtkMatrix4x4* intersectingXYToXY, double intersectionLineTip1[3], double intersectionLineTip2[3])
{
  // Define current slice plane
  double slicePlaneNormal[3] = { 0.,0.,1. };
  double slicePlaneOrigin[3] = { 0., 0., 0. };

  // Define slice size dimensions
  int* intersectingSliceSizeDimensions = intersectingSliceNode->GetDimensions();
  double intersectingPlaneOrigin[4] = { 0, 0, 0, 1 };
  double intersectingPlaneX[4] = { double(intersectingSliceSizeDimensions[0]), 0., 0., 1. };
  double intersectingPlaneY[4] = { 0., double(intersectingSliceSizeDimensions[1]), 0., 1. };
  intersectingXYToXY->MultiplyPoint(intersectingPlaneOrigin, intersectingPlaneOrigin);
  intersectingXYToXY->MultiplyPoint(intersectingPlaneX, intersectingPlaneX);
  intersectingXYToXY->MultiplyPoint(intersectingPlaneY, intersectingPlaneY);

  // Compute intersection
  int intersectionFound = this->IntersectWithFinitePlane(slicePlaneNormal, slicePlaneOrigin,
    intersectingPlaneOrigin, intersectingPlaneX, intersectingPlaneY, intersectionLineTip1, intersectionLineTip2);

  return intersectionFound;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetPipelinesHandlesVisibility(bool visible)
{
  // Force visible handles in the "handles always visible" mode is activated
  if (HANDLES_ALWAYS_VISIBLE)
    {
      visible = true;
    }

  // Update handles visibility in all display pipelines
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    (*sliceIntersectionIt)->SetHandlesVisibility(visible);
    }
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetPipelinesHandlesOpacity(double opacity)
{
  // Force visible handles in the "handles always visible" mode is activated
  if (HANDLES_ALWAYS_VISIBLE)
    {
    opacity = 1.0;
    }

  // Update handles visibility in all display pipelines
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    (*sliceIntersectionIt)->SetHandlesOpacity(opacity);
    }
}

//----------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionRepresentation::IsMouseCursorInSliceView(double cursorPosition[2])
{
  // Get current slice view bounds
  vtkMRMLSliceNode* currentSliceNode = this->GetSliceNode();
  double sliceViewBounds[4] = {};
  this->GetSliceViewBoundariesXY(currentSliceNode, sliceViewBounds);

  // Check mouse cursor position
  bool inSliceView;
  if ((cursorPosition[0] > sliceViewBounds[0]) &&
    (cursorPosition[0] < sliceViewBounds[1]) &&
    (cursorPosition[1] > sliceViewBounds[2]) &&
    (cursorPosition[1] < sliceViewBounds[3]))
    {
    inSliceView = true;
    }
  else
    {
    inSliceView = false;
    }
  return inSliceView;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::ComputeHandleToWorldTransformMatrix(double handlePosition[2], double handleOrientation[2],
  vtkMatrix4x4* handleToWorldTransformMatrix)
{
  // Reset handle to world transform
  handleToWorldTransformMatrix->Identity();

  // Get rotation matrix
  double handleOrientationDefault[2] = { SLICEOFSSET_HANDLE_DEFAULT_ORIENTATION[0],
                                         SLICEOFSSET_HANDLE_DEFAULT_ORIENTATION [1]};
  this->RotationMatrixFromVectors(handleOrientationDefault, handleOrientation, handleToWorldTransformMatrix);

  // Add translation to matrix
  double handleTranslation[2] = { handlePosition[0] - SLICEOFSSET_HANDLE_DEFAULT_POSITION[0],
                                  handlePosition[1] - SLICEOFSSET_HANDLE_DEFAULT_POSITION[1]};
  handleToWorldTransformMatrix->SetElement(0, 3, handleTranslation[0]); // Translation X
  handleToWorldTransformMatrix->SetElement(1, 3, handleTranslation[1]); // Translation Y
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::RotationMatrixFromVectors(double vector1[2], double vector2[2], vtkMatrix4x4* rotationMatrixHom)
{
  // 3D vectors
  double vector1_3D[3] = { vector1[0], vector1[1], 0.0};
  double vector2_3D[3] = { vector2[0], vector2[1], 0.0 };

  // Normalize input vectos
  vtkMath::Normalize(vector1_3D);
  vtkMath::Normalize(vector2_3D);

  // Cross and dot products
  double v[3];
  vtkMath::Cross(vector1_3D, vector2_3D, v);
  double c = vtkMath::Dot(vector1_3D, vector2_3D);
  double s = vtkMath::Norm(v);

  // Compute rotation matrix
  if (s == 0.0) // If vectors are aligned (i.e., cross product = 0)
    {
    if (c > 0.0) // Same direction
      {
      rotationMatrixHom->Identity();
      }
    else // Opposite direction
      {
      vtkNew<vtkTransform> transform;
      transform->RotateZ(180); // invert direction
      rotationMatrixHom->SetElement(0, 0, transform->GetMatrix()->GetElement(0, 0));
      rotationMatrixHom->SetElement(0, 1, transform->GetMatrix()->GetElement(0, 1));
      rotationMatrixHom->SetElement(0, 2, transform->GetMatrix()->GetElement(0, 2));
      rotationMatrixHom->SetElement(0, 3, 0.0);
      rotationMatrixHom->SetElement(1, 0, transform->GetMatrix()->GetElement(1, 0));
      rotationMatrixHom->SetElement(1, 1, transform->GetMatrix()->GetElement(1, 1));
      rotationMatrixHom->SetElement(1, 2, transform->GetMatrix()->GetElement(1, 2));
      rotationMatrixHom->SetElement(1, 3, 0.0);
      rotationMatrixHom->SetElement(2, 0, transform->GetMatrix()->GetElement(2, 0));
      rotationMatrixHom->SetElement(2, 1, transform->GetMatrix()->GetElement(2, 1));
      rotationMatrixHom->SetElement(2, 2, transform->GetMatrix()->GetElement(2, 2));
      rotationMatrixHom->SetElement(2, 3, 0.0);
      rotationMatrixHom->SetElement(3, 0, 0.0);
      rotationMatrixHom->SetElement(3, 1, 0.0);
      rotationMatrixHom->SetElement(3, 2, 0.0);
      rotationMatrixHom->SetElement(3, 3, 1.0);
      }
    }
  else // If vectors are not aligned
    {
    vtkNew<vtkMatrix3x3> rotationMatrix;
    vtkNew<vtkMatrix3x3> identityMatrix;
    vtkNew<vtkMatrix3x3> kmat;
    kmat->SetElement(0, 0, 0.0);
    kmat->SetElement(0, 1, -v[2]);
    kmat->SetElement(0, 2, v[1]);
    kmat->SetElement(1, 0, v[2]);
    kmat->SetElement(1, 1, 0.0);
    kmat->SetElement(1, 2, -v[0]);
    kmat->SetElement(2, 0, -v[1]);
    kmat->SetElement(2, 1, v[0]);
    kmat->SetElement(2, 2, 0.0);
    vtkNew<vtkMatrix3x3> kmat2;
    vtkMatrix3x3::Multiply3x3(kmat, kmat, kmat2);
    vtkNew<vtkMatrix3x3> kmat2x;
    rotationMatrix->SetElement(0, 0, identityMatrix->GetElement(0, 0) + kmat->GetElement(0, 0) + kmat2->GetElement(0, 0) * ((1 - c) / (pow(s, 2.0))));
    rotationMatrix->SetElement(0, 1, identityMatrix->GetElement(0, 1) + kmat->GetElement(0, 1) + kmat2->GetElement(0, 1) * ((1 - c) / (pow(s, 2.0))));
    rotationMatrix->SetElement(0, 2, identityMatrix->GetElement(0, 2) + kmat->GetElement(0, 2) + kmat2->GetElement(0, 2) * ((1 - c) / (pow(s, 2.0))));
    rotationMatrix->SetElement(1, 0, identityMatrix->GetElement(1, 0) + kmat->GetElement(1, 0) + kmat2->GetElement(1, 0) * ((1 - c) / (pow(s, 2.0))));
    rotationMatrix->SetElement(1, 1, identityMatrix->GetElement(1, 1) + kmat->GetElement(1, 1) + kmat2->GetElement(1, 1) * ((1 - c) / (pow(s, 2.0))));
    rotationMatrix->SetElement(1, 2, identityMatrix->GetElement(1, 2) + kmat->GetElement(1, 2) + kmat2->GetElement(1, 2) * ((1 - c) / (pow(s, 2.0))));
    rotationMatrix->SetElement(2, 0, identityMatrix->GetElement(2, 0) + kmat->GetElement(2, 0) + kmat2->GetElement(2, 0) * ((1 - c) / (pow(s, 2.0))));
    rotationMatrix->SetElement(2, 1, identityMatrix->GetElement(2, 1) + kmat->GetElement(2, 1) + kmat2->GetElement(2, 1) * ((1 - c) / (pow(s, 2.0))));
    rotationMatrix->SetElement(2, 2, identityMatrix->GetElement(2, 2) + kmat->GetElement(2, 2) + kmat2->GetElement(2, 2) * ((1 - c) / (pow(s, 2.0))));

    // Convert to 4x4 matrix
    rotationMatrixHom->SetElement(0, 0, rotationMatrix->GetElement(0, 0));
    rotationMatrixHom->SetElement(0, 1, rotationMatrix->GetElement(0, 1));
    rotationMatrixHom->SetElement(0, 2, rotationMatrix->GetElement(0, 2));
    rotationMatrixHom->SetElement(0, 3, 0.0);
    rotationMatrixHom->SetElement(1, 0, rotationMatrix->GetElement(1, 0));
    rotationMatrixHom->SetElement(1, 1, rotationMatrix->GetElement(1, 1));
    rotationMatrixHom->SetElement(1, 2, rotationMatrix->GetElement(1, 2));
    rotationMatrixHom->SetElement(1, 3, 0.0);
    rotationMatrixHom->SetElement(2, 0, rotationMatrix->GetElement(2, 0));
    rotationMatrixHom->SetElement(2, 1, rotationMatrix->GetElement(2, 1));
    rotationMatrixHom->SetElement(2, 2, rotationMatrix->GetElement(2, 2));
    rotationMatrixHom->SetElement(2, 3, 0.0);
    rotationMatrixHom->SetElement(3, 0, 0.0);
    rotationMatrixHom->SetElement(3, 1, 0.0);
    rotationMatrixHom->SetElement(3, 2, 0.0);
    rotationMatrixHom->SetElement(3, 3, 1.0);
    }
}
