// Copyright (c) Choreo contributors

package choreo;

import choreo.trajectory.ProjectFile;
import com.google.gson.Gson;
import org.junit.jupiter.api.Test;

// import static org.junit.jupiter.api.Assertions.*;

// import choreo.trajectory.SwerveSample;
// import com.google.gson.Gson;
// import org.junit.jupiter.api.BeforeAll;
// import org.junit.jupiter.api.Test;

public class ChoreoTests {
  public static final String TRAJ =
      "{"
          + "  \"name\":\"New Path (1)\","
          + "  \"version\":\"v2025.0.0\","
          + "  \"traj\":{"
          + "   \"waypoints\":[0.0,1.5256324024471957],"
          + "   \"samples\":["
          + "      {\"t\":0.0, \"x\":4.060854911804199, \"y\":5.9335150718688965, \"heading\":0.0, \"vx\":0.0, \"vy\":0.0, \"omega\":0.0, \"ax\":0.0, \"ay\":0.0, \"alpha\":0.0, \"fx\":[0.0,0.0,0.0,0.0], \"fy\":[0.0,0.0,0.0,0.0]},"
          + "      {\"t\":0.08475735569151086, \"x\":4.156365657180302, \"y\":5.915360137036969, \"heading\":0.0, \"vx\":0.7512484283076725, \"vy\":-0.14279928614111795, \"omega\":0.0, \"ax\":8.86351894981127, \"ay\":-1.6848011004596155, \"alpha\":0.0, \"fx\":[150.76592126193077,150.76592126193077,150.76592126193077,150.76592126193077], \"fy\":[-28.657984655103263,-28.657984655103263,-28.657984655103263,-28.657984655103256]},"
          + "      {\"t\":0.1695147113830217, \"x\":4.315540531153555, \"y\":5.885103756432743, \"heading\":0.0, \"vx\":1.5024205473854988, \"vy\":-0.28558406722240554, \"omega\":0.0, \"ax\":8.862618624060529, \"ay\":-1.6846299641644673, \"alpha\":0.0, \"fx\":[150.7506069785158,150.7506069785158,150.7506069785158,150.7506069785158], \"fy\":[-28.6550736756891,-28.655073675689092,-28.65507367568909,-28.655073675689092]},"
          + "      {\"t\":0.25427206707453254, \"x\":4.538368211993072, \"y\":5.842748082121665, \"heading\":0.0, \"vx\":2.253478177931421, \"vy\":-0.4283470860217575, \"omega\":0.0, \"ax\":8.861267844165718, \"ay\":-1.6843732043614048, \"alpha\":0.0, \"fx\":[150.72763059899725,150.72763059899728,150.72763059899728,150.72763059899725], \"fy\":[-28.6507062649046,-28.65070626490461,-28.650706264904603,-28.650706264904596]},"
          + "      {\"t\":0.3390294227660434, \"x\":4.824829285899579, \"y\":5.788296804328294, \"heading\":0.0, \"vx\":3.0043449446578983, \"vy\":-0.5710738249230501, \"omega\":0.0, \"ax\":8.859015959144662, \"ay\":-1.683945160118964, \"alpha\":0.0, \"fx\":[150.6893266791094,150.6893266791094,150.6893266791094,150.6893266791094], \"fy\":[-28.643425354814635,-28.64342535481464,-28.643425354814635,-28.64342535481464]},"
          + "      {\"t\":0.4237867784575543, \"x\":5.174883297993235, \"y\":5.721757612824412, \"heading\":0.0, \"vx\":3.7548298885829294, \"vy\":-0.7137279859388952, \"omega\":0.0, \"ax\":8.85451106633179, \"ay\":-1.6830888582079926, \"alpha\":0.0, \"fx\":[150.61269974132443,150.61269974132443,150.61269974132443,150.61269974132443], \"fy\":[-28.62885990431864,-28.628859904318645,-28.62885990431864,-28.62885990431863]},"
          + "      {\"t\":0.5085441341490652, \"x\":5.5884008510014125, \"y\":5.643155103784129, \"heading\":0.0, \"vx\":4.504169772232789, \"vy\":-0.856164490876266, \"omega\":0.0, \"ax\":8.841001203045256, \"ay\":-1.6805208677472103, \"alpha\":0.0, \"fx\":[150.38290083233056,150.38290083233056,150.38290083233056,150.38290083233056], \"fy\":[-28.58517912134676,-28.58517912134677,-28.58517912134676,-28.58517912134676]},"
          + "      {\"t\":0.5933014898405761, \"x\":6.010554921806329, \"y\":5.562910942617175, \"heading\":0.0, \"vx\":4.821881047318938, \"vy\":-0.916555889519671, \"omega\":0.0, \"ax\":3.7484802647058064, \"ay\":-0.7125210331657083, \"alpha\":0.0, \"fx\":[63.76057676873007,63.76057676873007,63.76057676873007,63.76057676873006], \"fy\":[-12.11977890406808,-12.119778904068076,-12.119778904068076,-12.119778904068085]},"
          + "      {\"t\":0.678058845532087, \"x\":6.419245719901963, \"y\":5.485225915910452, \"heading\":0.0, \"vx\":4.821888213403624, \"vy\":-0.9165572515870056, \"omega\":0.0, \"ax\":0.00008454827064418903, \"ay\":-0.000016070203300175502, \"alpha\":0.0, \"fx\":[0.0014381418922857548,0.0014381418922857548,0.0014381418922816747,0.0014381418922816747], \"fy\":[-0.00027334956005106714,-0.0002733495600470634,-0.0002733495600470634,-0.00027334956005106714]},"
          + "      {\"t\":0.7628162012235978, \"x\":6.8279362143305455, \"y\":5.407540946925889, \"heading\":0.0, \"vx\":4.821888213556462, \"vy\":-0.9165572515852092, \"omega\":0.0, \"ax\":1.8009720242906364e-9, \"ay\":2.160826031993857e-11, \"alpha\":0.0, \"fx\":[3.0634007821541895e-8,3.0634007821541895e-8,3.063400583419931e-8,3.063400583419931e-8], \"fy\":[3.675492252675112e-10,3.6755122101266025e-10,3.675512206873995e-10,3.675492250506707e-10]},"
          + "      {\"t\":0.8475735569151087, \"x\":7.236626708732319, \"y\":5.329855977943633, \"heading\":0.0, \"vx\":4.821888213406692, \"vy\":-0.9165572515707732, \"omega\":0.0, \"ax\":-1.7647679599491275e-9, \"ay\":1.6991199737539487e-10, \"alpha\":0.0, \"fx\":[-3.001820188301235e-8,-3.001820188301235e-8,-3.001819422637661e-8,-3.001819422637661e-8], \"fy\":[2.890158287390323e-9,2.890150630971422e-9,2.890150630971422e-9,2.890158287390323e-9]},"
          + "      {\"t\":0.9323309126066196, \"x\":7.645316292070791, \"y\":5.2521711821132495, \"heading\":0.0, \"vx\":4.821881047299351, \"vy\":-0.9165558896229824, \"omega\":0.0, \"ax\":-0.00008454853791252381, \"ay\":0.000016068792925837128, \"alpha\":0.0, \"fx\":[-0.0014381464385700982,-0.0014381464385697929,-0.001438146438569654,-0.001438146438569654], \"fy\":[0.00027332556999565966,0.00027332556999534047,0.00027332556999534047,0.00027332556999565966]},"
          + "      {\"t\":1.0170882682981304, \"x\":8.013613627779863, \"y\":5.1821642514641395, \"heading\":0.0, \"vx\":4.504169772237168, \"vy\":-0.8561644908556492, \"omega\":0.0, \"ax\":-3.748480264423064, \"ay\":0.7125210346278033, \"alpha\":0.0, \"fx\":[-63.760576763920675,-63.760576763920675,-63.76057676392065,-63.760576763920646], \"fy\":[12.119778928937924,12.11977892893788,12.11977892893788,12.119778928937924]},"
          + "      {\"t\":1.1018456239896413, \"x\":8.300107046680969, \"y\":5.127706825449855, \"heading\":0.0, \"vx\":3.7548298885853257, \"vy\":-0.7137279859263548, \"omega\":0.0, \"ax\":-8.841001203068696, \"ay\":1.6805208676519356, \"alpha\":0.0, \"fx\":[-150.38290083272904,-150.38290083272904,-150.38290083272904,-150.38290083272904], \"fy\":[28.58517911972625,28.585179119726256,28.585179119726252,28.58517911972625]},"
          + "      {\"t\":1.186602979681152, \"x\":8.522942820108517, \"y\":5.085349612878347, \"heading\":0.0, \"vx\":3.004344944659468, \"vy\":-0.5710738249147432, \"omega\":0.0, \"ax\":-8.854511066341479, \"ay\":1.683088858158046, \"alpha\":0.0, \"fx\":[-150.61269974148968,-150.61269974148968,-150.61269974148968,-150.61269974148968], \"fy\":[28.628859903468815,28.628859903468815,28.62885990346882,28.62885990346882]},"
          + "      {\"t\":1.271360335372663, \"x\":8.68212093076613, \"y\":5.055092617037148, \"heading\":0.0, \"vx\":2.2534781779324087, \"vy\":-0.42834708601653704, \"omega\":0.0, \"ax\":-8.859015959151549, \"ay\":1.6839451600825526, \"alpha\":0.0, \"fx\":[-150.68932667922715,-150.68932667922715,-150.68932667922715,-150.68932667922715], \"fy\":[28.643425354195287,28.643425354195298,28.6434253541953,28.64342535419529]},"
          + "      {\"t\":1.356117691064174, \"x\":8.77763329413119, \"y\":5.036937374654188, \"heading\":0.0, \"vx\":1.5024205473861054, \"vy\":-0.28558406721918767, \"omega\":0.0, \"ax\":-8.8612678441702, \"ay\":1.6843732043377817, \"alpha\":0.0, \"fx\":[-150.7276305990732,-150.7276305990732,-150.7276305990732,-150.7276305990732], \"fy\":[28.650706264502567,28.65070626450257,28.650706264502578,28.650706264502563]},"
          + "      {\"t\":1.4408750467556848, \"x\":8.809473443140519, \"y\":5.030885115004814, \"heading\":0.0, \"vx\":0.7512484283079355, \"vy\":-0.14279928613971668, \"omega\":0.0, \"ax\":-8.862618624064584, \"ay\":1.6846299641430351, \"alpha\":0.0, \"fx\":[-150.75060697858478,-150.75060697858478,-150.75060697858478,-150.75060697858478], \"fy\":[28.65507367532453,28.655073675324537,28.65507367532453,28.655073675324537]},"
          + "      {\"t\":1.5256324024471957, \"x\":8.777636528015137, \"y\":5.0369367599487305, \"heading\":0.0, \"vx\":0.0, \"vy\":0.0, \"omega\":0.0, \"ax\":-8.863518949814408, \"ay\":1.6848011004430885, \"alpha\":0.0, \"fx\":[-150.765921261984,-150.765921261984,-150.765921261984,-150.765921261984], \"fy\":[28.65798465482203,28.65798465482204,28.65798465482204,28.65798465482203]}],"
          + "   \"splits\":[],"
          + "   \"forcesAvailable\":false"
          + "  },"
          + "  \"events\":[]"
          + "}";

  public static final String PROJ =
      "{"
          + " \"name\": \"idk\","
          + " \"version\": \"v2025.0.0\","
          + " \"type\": \"Swerve\","
          + " \"variables\": {"
          + "   \"expressions\": {},"
          + "   \"poses\": {}"
          + " },"
          + " \"config\": {"
          + "   frontLeft: {"
          + "     \"x\": {"
          + "       \"exp\": \"11 in\","
          + "       \"val\": \"0.2794\""
          + "     },"
          + "     \"y\": {"
          + "       \"exp\": \"11 in\","
          + "       \"val\": \"0.2794\""
          + "     }"
          + "   },"
          + "   backLeft: {"
          + "     \"x\": {"
          + "       \"exp\": \"-11 in\","
          + "       \"val\": \"-0.2794\""
          + "     },"
          + "     \"y\": {"
          + "       \"exp\": \"11 in\","
          + "       \"val\": \"0.2794\""
          + "     }"
          + "   },"
          + "   \"mass\": {"
          + "     \"exp\": \"150 lbs\","
          + "     \"val\": 68.0388555"
          + "   },"
          + "   \"inertia\": {"
          + "     \"exp\": \"6 kg m ^ 2\","
          + "     \"val\": 6.0"
          + "   },"
          + "   \"gearing\": {"
          + "     \"exp\": \"6.5\","
          + "     \"val\": 6.5"
          + "   },"
          + "   \"radius\": {"
          + "     \"exp\": \"2 in\","
          + "     \"val\": 0.0508"
          + "   },"
          + "   \"vmax\": {"
          + "     \"exp\": \"6000 RPM\","
          + "     \"val\": 628.318530717"
          + "   },"
          + "   \"tmax\": {"
          + "     \"exp\": \"1.2 N * m\","
          + "     \"val\": 1.2"
          + "   },"
          + "   \"bumper\": {"
          + "     \"front\": {"
          + "       \"exp\": \"16 in\","
          + "       \"val\": 0.4064"
          + "     },"
          + "     \"side\": {"
          + "       \"exp\": \"16 in\","
          + "       \"val\": 0.4064"
          + "     },"
          + "     \"back\": {"
          + "       \"exp\": \"16 in\","
          + "       \"val\": 0.4064"
          + "     },"
          + "   },"
          + "   \"differentialTrackWidth\": {"
          + "     \"exp\": \"24 in\","
          + "     \"val\": 0.6096"
          + "   }"
          + " },"
          + " \"generationFeatures\": []"
          + "}";

  private static final Gson GSON = new Gson();

  @Test
  public void testChoreo() {
    ProjectFile projectFile = GSON.fromJson(PROJ, ProjectFile.class);
    System.out.println(projectFile.name);
    System.out.println(Choreo.readTrajectoryString(TRAJ, projectFile).name());
  }
}
