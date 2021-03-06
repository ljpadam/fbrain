/*==========================================================================
  
  © Université de Strasbourg - Centre National de la Recherche Scientifique
  
  Date: 27/05/2013
  Author(s):Marc Schweitzer (marc.schweitzer(at)unistra.fr)
  
  This software is governed by the CeCILL-B license under French law and
  abiding by the rules of distribution of free software.  You can  use,
  modify and/ or redistribute the software under the terms of the CeCILL-B
  license as circulated by CEA, CNRS and INRIA at the following URL
  "http://www.cecill.info".
  
  As a counterpart to the access to the source code and  rights to copy,
  modify and redistribute granted by the license, users are provided only
  with a limited warranty  and the software's author,  the holder of the
  economic rights,  and the successive licensors  have only  limited
  liability.
  
  In this respect, the user's attention is drawn to the risks associated
  with loading,  using,  modifying and/or developing or reproducing the
  software by the user in light of its specific status of free software,
  that may mean  that it is complicated to manipulate,  and  that  also
  therefore means  that it is reserved for developers  and  experienced
  professionals having in-depth computer knowledge. Users are therefore
  encouraged to load and test the software's suitability as regards their
  requirements in conditions enabling the security of their systems and/or
  data to be ensured and,  more generally, to use and operate it in the
  same conditions as regards security.
  
  The fact that you are presently reading this means that you have had
  knowledge of the CeCILL-B license and that you accept its terms.
  
==========================================================================*/

#ifndef BTKSRHMATRIXCOMPUTATION_H
#define BTKSRHMATRIXCOMPUTATION_H

#include "itkObject.h"
#include "itkTransform.h"
#include "itkImageMaskSpatialObject.h"
#include "itkImageRegionConstIteratorWithIndex.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "vnl/vnl_sparse_matrix.h"
#include "itkBSplineInterpolationWeightFunction.h"



#include "btkMacro.h"
#include "btkOrientedSpatialFunction.h"
#include "btkLinearInterpolateImageFunctionWithWeights.h"
#include "btkPSF.h"
#include "btkGaussianPSF.h"
#include "btkBoxCarPSF.h"
#include "btkSincPSF.h"
#include "btkHybridPSF.h"
#include "btkImageHelper.h"


#include "iostream"
#include "sstream"
#include "limits.h"


namespace btk
{
/**
 * @class SRHMatrixComputation
 * @brief SRHMatrixComputation compute H matrix with a set of low resolution images,
 * the corresponding transformations and a type of PSF.
 *
 * In order to manage memory H should be created and allocated outside this class and
 * then given by reference with SetH method
 * @author Marc Schweitzer
 * @ingroup SuperResolution
 *
 */
template < class TImage >
class SRHMatrixComputation: public itk::Object
{
    public:
        /** Typdefs */
        typedef btk::SRHMatrixComputation< TImage >      Self;
        typedef itk::Object                     Superclass;
        typedef itk::SmartPointer< Self >       Pointer;
        typedef itk::SmartPointer< const Self > ConstPointer;

        typedef TImage                          ImageType;
        typedef typename ImageType::RegionType  RegionType;
        typedef typename ImageType::SizeType    SizeType;
        typedef typename ImageType::IndexType   IndexType;
        typedef typename ImageType::SpacingType SpacingType;
        typedef typename ImageType::PointType   PointType;

        typedef float                           PrecisionType;

        typedef ContinuousIndex<double, TImage::ImageDimension> ContinuousIndexType;

        typedef itk::ImageMaskSpatialObject< TImage::ImageDimension > MaskType;
        typedef typename MaskType::Pointer   MaskPointer;

        typedef itk::Transform< double >        TransformType;

        /** Oriented spatial function typedef. */
        typedef OrientedSpatialFunction<double, 3, PointType> FunctionType;

        typedef LinearInterpolateImageFunctionWithWeights<ImageType, double> InterpolatorType;
        typedef typename InterpolatorType::Pointer InterpolatorPointer;

        /** Const iterator typedef. */
        typedef ImageRegionConstIteratorWithIndex< ImageType >  ConstIteratorType;

        /** Const iterator typedef. */
        typedef ImageRegionIteratorWithIndex< ImageType >  IteratorType;

        typedef itk::Image< float, 3 >  PsfImageType;

        /** New macro */
        itkNewMacro(Self);

        /** Run-time type information (and related methods). */
        itkTypeMacro(btk::SRHMatrixComputation, itk::Object);

        /** Update method (compute H) */
        virtual void Update();

        btkSetMacro(Images, std::vector< typename ImageType::Pointer >);
        btkGetMacro(Images, std::vector< typename ImageType::Pointer >);

        btkSetMacro(Masks, std::vector< typename MaskType::Pointer >);
        btkGetMacro(Masks, std::vector< typename MaskType::Pointer >);

        btkSetMacro(ReferenceImage,typename ImageType::Pointer);
        btkGetMacro(ReferenceImage,typename ImageType::Pointer);

        btkSetMacro(Transforms, std::vector< typename TransformType::Pointer >);
        btkGetMacro(Transforms, std::vector< typename TransformType::Pointer >);

        btkSetMacro(InverseTransforms, std::vector< typename TransformType::Pointer >);
        btkGetMacro(InverseTransforms, std::vector< typename TransformType::Pointer >);

        btkSetMacro(H,vnl_sparse_matrix< PrecisionType >*);

        btkSetMacro(PSF,btk::PSF::Pointer);

        btkSetMacro(Y,vnl_vector< PrecisionType >*);

        /**
         * @brief SetOutliers
         * @param _outliers is a vector of vector of boolean
         * @todo Outliers are not implemented yet !
         */
        void SetOutliers(std::vector< std::vector< bool > > _outliers)
        {
            m_Outliers = _outliers;
            m_UseOutliers = true;
        }

        void TestFillingOfY(); // for testing purpose
        void TestFillingOfX(); // for testing purpose
        void SimulateY();      //for testing purpose


    protected:

        /** Initialize  */
        virtual void Initialize();
        /** Constructor */
        SRHMatrixComputation();
        /**  Destructor*/
        virtual ~SRHMatrixComputation(){}
        /** ToBeRemoved */
        void ComputeH();

    private:

        struct img_size
        {
           unsigned int width;
           unsigned int height;
           unsigned int depth;
        }m_XSize;

        vnl_sparse_matrix< PrecisionType >* m_H;
        vnl_vector< PrecisionType >* m_Y;
        vnl_vector< PrecisionType > m_SimY;
        vnl_vector< PrecisionType > m_HtY;
        vnl_vector< PrecisionType > m_X;


        std::vector< typename ImageType::Pointer > m_Images;
        std::vector< typename ImageType::Pointer > m_ImagesFilledWithY; //for testing purpose
        std::vector< typename ImageType::Pointer > m_SimulatedImages;
        std::vector< typename MaskType::Pointer >  m_Masks;
        std::vector< RegionType >                  m_Regions;

        typename ImageType::Pointer           m_ReferenceImage;
        RegionType m_OutputImageRegion;

        std::vector< typename TransformType::Pointer > m_Transforms;
        std::vector< typename TransformType::Pointer > m_InverseTransforms;

        std::vector< std::vector< bool > > m_Outliers;
        bool                               m_UseOutliers;

        PSF::Pointer                       m_PSF;

        std::vector< PSF::Pointer >        m_PSFs;

        bool                               m_IsHComputed;

        unsigned int m_NumberOfLRImages;
};
}

#ifndef ITK_MANUAL_INSTANTIATION
#include "btkSRHMatrixComputation.txx"
#endif

#endif // BTKSRHMATRIXCOMPUTATION_H
