Rendering water using Gerstner waves.
With texture, VAO, normal calculating and Ward's Anisotropic lighting model.
Read about it in my blog:

  http://johnhany.net/2014/02/water-rendering-with-gerstner-wave/

Environment requirements:

  Visual Studio 2010
  OpenGL 3.0+
  GLSL 1.1+
  freeglut 2.8.1
  glew 1.10
  GLM 0.9.5.1

Please replace the 254th to the 264th lines in "Include/glm/gtc/matrix_transform.inl" with the following content:

        valType const rad = glm::radians(fovy);
#endif
 
        valType tanHalfFovy = tan(rad);
 
        detail::tmat4x4<valType, defaultp> Result(valType(0));
	Result[0][0] = valType(1) / (tanHalfFovy);
	Result[1][1] = (aspect) / (tanHalfFovy);
	Result[2][2] = - (zFar + zNear) / (zFar - zNear);
	Result[2][3] = - (valType(2) * zFar * zNear) / (zFar - zNear);
	Result[3][2] = - valType(1);




