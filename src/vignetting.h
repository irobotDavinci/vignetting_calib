#ifndef VIGNETTING_H
#define VIGNETTING_H

#include <camera_model/camera_models/CameraFactory.h>
#include <ceres/ceres.h>
#include <iostream>
#include <opencv2/opencv.hpp>

namespace camera_model
{

class vignetting
{
    class luminanceError
    {
        public:
        luminanceError( double _value, double _r )
        : value( _value )
        , r( _r )
        {
        }

        /* clang-format off */
        template < typename T >
        T luminance(T k0, T k1, T k2, T k3/*, T k4, T k5, T k6, T k7*/ ) const
        {
            return k0
                + k1 * r
                + k2 * r * r
                + k3 * r * r * r;
//                + k4 * r * r * r * r
//                + k5 * r * r * r * r * r
//                + k6 * r * r * r * r * r * r
//                + k7 * r * r * r * r * r * r * r;
        }
        /* clang-format on */

        template < typename T >
        bool operator( )( const T* const k, T* residuals ) const
        {
            T value_esti = luminance( k[0], k[1], k[2], k[3] /*, k[4], k[5], k[6]*/ );

            residuals[0] = T( value ) - value_esti;

            return true;
        }

        public:
        double value;
        double r;
    };

    public:
    vignetting( ) {}
    vignetting( std::string camera_model_file, cv::Size boardSize )
    : chessbordSize( boardSize )
    {
        cam = camera_model::CameraFactory::instance( )->generateCameraFromYamlFile( camera_model_file );

        //        image_size = cv::Size( cam->imageWidth( ), cam->imageHeight( ) );
        image_size = cv::Size( 640, 480 );

        center( 0 ) = image_size.width / 2;
        center( 1 ) = image_size.height / 2;
    }

    public:
    void readin_points( const std::vector< std::pair< cv::Point2d, double > > points )
    {
        points_num = points.size( );
        values.clear( );
        rs.clear( );

        for ( int point_index = 0; point_index < points_num; ++point_index )
        {
            double r = sqrt( ( points[point_index].first.x - center( 0 ) )
                             * ( points[point_index].first.x - center( 0 ) )
                             + ( points[point_index].first.y - center( 1 ) )
                               * ( points[point_index].first.y - center( 1 ) ) );

            rs.push_back( r );
            values.push_back( points[point_index].second );
        }
    }

    double add( int raw_index, int col_index, double value ) {}
    double get( int xx, int yy )
    {
        double dis = distance( double( xx ), double( yy ), center( 0 ), center( 1 ) );
        //        std::cout << " dis " << dis << std::endl;
        double r
        = params[0] + params[1] * dis + params[2] * dis * dis + params[3] * dis * dis * dis; /*
            + params[3] * dis * dis * dis * dis + params[4] * dis * dis * dis * dis * dis
            + params[5] * dis * dis * dis * dis * dis * dis
            + params[6] * dis * dis * dis * dis * dis * dis * dis*/

        return r;
    }
    cv::Mat remove( const cv::Mat image_in )
    {
        cv::Mat image_tmp( image_in.rows, image_in.cols, CV_8UC1 );
        for ( int raw_index = 0; raw_index < image_size.height; ++raw_index )
            for ( int col_index = 0; col_index < image_size.width; ++col_index )
            {
                double feed      = params[0] / get( col_index, raw_index );
                int velue        = image_in.at< uchar >( raw_index, col_index );
                int valuw_feeded = velue * feed;

                image_tmp.at< uchar >( raw_index, col_index ) = valuw_feeded;
            }
        return image_tmp;
    }
    void resualt( )
    {
        cv::Mat image( image_size, CV_8UC1 );

        for ( int raw_index = 0; raw_index < image_size.height; ++raw_index )
            for ( int col_index = 0; col_index < image_size.width; ++col_index )
            {
                double value = get( col_index, raw_index );
                //                std::cout << " value " << value << std::endl;
                image.at< uchar >( raw_index, col_index ) = value;
            }
        cv::imshow( "resualt", image );
        cv::waitKey( 0 );
    }

    void solve( )
    {

        if ( rs.size( ) == values.size( ) )
            points_num = rs.size( );

        double poly_k[] = { 0.0, 1.0, 0.0, 0.0 /*, 0.0, 0.0, 0.0, 0.0*/ };

        ceres::Problem problem;

        for ( int i = 0; i < points_num; ++i )
        {
            ceres::CostFunction* costFunction
            = new ceres::AutoDiffCostFunction< luminanceError, 1, 4 >(
            new luminanceError( values[i], rs[i] ) );

            problem.AddResidualBlock( costFunction, NULL /* squared loss */, poly_k );
        }

        ceres::Solver::Options options;
        options.minimizer_progress_to_stdout = true;
        options.trust_region_strategy_type   = ceres::DOGLEG;
        ceres::Solver::Summary summary;
        ceres::Solve( options, &problem, &summary );

        for ( int index   = 0; index < 7; ++index )
            params[index] = poly_k[index];

        std::cout << params[0] << " " << params[1] << " " << params[2] << " " << params[3]
                  /*
    << " " << params[4] << " " << params[5] << " " << params[6] */
                  << std::endl;
    }

    template < typename T >
    T distance( T x1, T y1, T x2, T y2 )
    {
        return sqrt( ( x1 - x2 ) * ( x1 - x2 ) + ( y1 - y2 ) * ( y1 - y2 ) );
    }

    cv::Mat draw( )
    {
        cv::Mat image_show( image_size, CV_8UC3 );

        for ( int row_index = 0; row_index < image_show.rows; ++row_index )
            for ( int col_index = 0; col_index < image_show.cols; ++col_index )
            {
                double r
                = distance( double( col_index ), double( row_index ), center( 0 ), center( 1 ) );
            }
    }

    cv::Mat showPoly( ) { cv::Mat poly_image( ); }

    cv::Mat drawPoints( cv::Mat image_in, std::vector< cv::Point2f > points )
    {
        int drawShiftBits  = 4;
        int drawMultiplier = 1 << drawShiftBits;
        cv::Scalar yellow( 0, 255, 255 );
        cv::Scalar green( 0, 255, 0 );
        cv::Scalar red( 0, 0, 255 );

        //        for ( size_t i = 0; i < images.size( ); ++i )
        //        {
        cv::Mat image = image_in;
        cv::Mat image_color;
        if ( image.channels( ) == 1 )
        {
            cv::cvtColor( image, image_color, CV_GRAY2RGB );
        }
        else
        {
            image.copyTo( image_color );
        }

        // for ( int point_index = 0; point_index < points.size( ); ++point_index )
        // {
        //     std::ostringstream oss;
        //     oss << point_index;
        //     cv::circle( image_color,
        //                 cv::Point( cvRound( points[point_index].x * drawMultiplier ),
        //                            cvRound( points[point_index].y * drawMultiplier ) ),
        //                 3, yellow, 2, CV_AA, drawShiftBits );
        //
        //     cv::putText( image_color, oss.str( ),
        //                  cv::Point( points[point_index].x, points[point_index].y ),
        //                  cv::FONT_HERSHEY_COMPLEX, 0.5, yellow, 1, CV_AA );
        // }

        //        std::cout << " points.size( ) " << points.size( ) << std::endl;

        int cnt = 0;
        for ( int raw_index = 0; raw_index < chessbordSize.height - 1; ++raw_index )
            for ( int col_index = 0; col_index < chessbordSize.width - 1; ++col_index )
            {
                int raw_index_add = chessbordSize.width * raw_index;
                ++cnt;
                double x = ( points[raw_index_add + col_index].x
                             + points[raw_index_add + col_index + 1].x
                             + points[raw_index_add + col_index + chessbordSize.width].x
                             + points[raw_index_add + col_index + chessbordSize.width + 1].x )
                           / 4;
                double y = ( points[raw_index_add + col_index].y
                             + points[raw_index_add + col_index + 1].y
                             + points[raw_index_add + col_index + chessbordSize.width].y
                             + points[raw_index_add + col_index + chessbordSize.width + 1].y )
                           / 4;

                int black_max = 70;
                int value     = image.at< uchar >( y, x );

                if ( value < black_max )
                    cv::circle( image_color, cv::Point( cvRound( x * drawMultiplier ),
                                                        cvRound( y * drawMultiplier ) ),
                                5, red, 2, CV_AA, drawShiftBits );
                else
                {
                    //  std::cout << " value " << value << std::endl;

                    values.push_back( value );
                    double new_r = distance( x, y, center( 0 ), center( 1 ) );
                    rs.push_back( new_r );

                    cv::circle( image_color, cv::Point( cvRound( x * drawMultiplier ),
                                                        cvRound( y * drawMultiplier ) ),
                                5, green, 2, CV_AA, drawShiftBits );
                }
            }

        cv::namedWindow( "image22", cv::WINDOW_NORMAL );
        cv::imshow( "image22 ", image_color );

        return image_color;
        //        }
    }

    public:
    camera_model::CameraPtr cam;
    double params[7];

    Eigen::Vector2d center;
    cv::Size image_size;
    cv::Size chessbordSize;

    int points_num;
    std::vector< double > values;
    std::vector< double > rs;
};
}

#endif // VIGNETTING_H
